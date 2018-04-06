#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <syslog.h>
#include <arpa/inet.h>

#include "../util/net.h"
#include "../util/epoller.h"
#include "../util/log.h"
#include "../crypto/crypto.h"

#include "msg.h"
#include "pub.h"
#include "tunnel.h"
#include "user.h"
#include "uvpnd.h"
#include "client.h"


extern struct uvpn_opts uvpn_opt;

#define CLINET_STATE_INITIAL        0
#define CLINET_STATE_AUTHSTARTUP    1
#define CLINET_STATE_RECVSHAKE_2    2
#define CLIENT_STATE_RESPONSE       3
#define CLIENT_STATE_IPASSIGNED     4

struct uvpn_client_run{
	struct epoller epoller;
	int state; 
	int linkfd;
    int udpfd;
    int tunfd;
    struct in_addr cli_vaddr;
    struct in_addr srv_vaddr;
    struct cryptor cryptor;
    struct cli_opts* opt;
};

struct uvpn_client_run g_client;

/* client terminated 
   @active: client active terminate
*/
static void client_terminated(int active)
{
  	if (active)
  	{
  		link_sendmsg(g_client.linkfd, TERMINATED, NULL, 0);
  	}  
  	
  	epoller_del(g_client.linkfd, &g_client.epoller);
    
	close(g_client.linkfd);

	close(g_client.tunfd);

	exit(0);
}

static int client_recv_shake_2(void* payload, int len, void* arg)
{	
	struct msg_shakehand* msg = (struct msg_shakehand* )payload;

	if (g_client.state != CLINET_STATE_AUTHSTARTUP)
	{
		uvpn_syslog(LOG_ERR,"recv err1");
		return -1;
	}
	
	uvpn_syslog(LOG_INFO,"client receive AUTH_SHAKEHAND_2 ACK userid = %d opt=%d", msg->userid, msg->authmethod);

	g_client.opt->authmethod = msg->authmethod;
    
	struct msg_shakehand ack;

	ack.userid  = msg->userid;
	ack.authmethod = msg->authmethod;
	
	link_sendmsg(g_client.linkfd, AUTH_SHAKEHAND_3, &ack, sizeof(ack));

	g_client.state = CLINET_STATE_RECVSHAKE_2;
	
	return 0;
}

static int client_recv_challenge(void* payload, int len, void* arg)
{
//	struct msg_chanresp* msg = (struct msg_chanresp* )payload;
	uint8_t* challenge = (uint8_t* )payload;
    uint8_t response[CRYPTO_MAX_SIZE];
	uint32_t outlen; 

	if (g_client.state != CLINET_STATE_RECVSHAKE_2)
	{
		uvpn_syslog(LOG_ERR,"recv err2");
		return -1;
	}

	simple_encrypt(challenge, response, len, g_client.opt->passwd, strlen(g_client.opt->passwd), &outlen);

	link_sendmsg(g_client.linkfd, AUTH_RESPONSE, response, outlen);

	g_client.state = CLIENT_STATE_RESPONSE;

	uvpn_syslog(LOG_INFO,"client change to CLIENT_STATE_RESPONSE");
	
	return 0;
}

static int _client_tun_read(int tunfd)
{
	int len;
	uint8_t buf[FRAME_SIZE];
	uint8_t* frame;
    
    /* read from tun dev, put it in buf[PRE_DATA_OFFSET] */
	len = read(tunfd, &buf[PRE_DATA_OFFSET], sizeof(buf));
    if (len < 0)
    {
         uvpn_syslog(LOG_ERR,"tunread failed on tunfd %d", tunfd);
         return -1;
    }

    frame = &buf[PRE_DATA_OFFSET];
    *((int*)&buf[0]) = len;

   	debug_packet("tun read preencrypt", buf, len + PRE_DATA_OFFSET);

    if (g_client.cryptor.engine)
    {
    	
         cryptor_encrypt(&g_client.cryptor, frame, len, &len);
    }

   	debug_packet("tun read postencrypt", buf, len + PRE_DATA_OFFSET);

    link_sendmsg(g_client.linkfd, ETHERNET_DATA, buf, len + PRE_DATA_OFFSET);

    uvpn_syslog(LOG_INFO,"tunrecv %d bytes and send to peer", len);
   
    return 0;
}

int client_tun_read(int event, void* handle)
{
	struct epoller_handler* handler = (struct epoller_handler*) handle;
	int tunfd  = handler->fd;

	if (0 != (EPOLLIN & event))
	{
		_client_tun_read(tunfd);
	}

    /*
	if ((0 != (EPOLLERR & event)) || (0 != (EPOLLHUP & event)))
	{
	 //   uvpn_syslog(LOG_INFO,"client local fd %d closed", fd);
		//close(fd);
	}
    */
    
	return 0;
}


static int client_recv_addr(void* payload, int len, void* arg)
{
    struct msg_addr* msg = (struct msg_addr* )payload;

    g_client.state = CLIENT_STATE_IPASSIGNED;
    
    g_client.srv_vaddr = msg->srv_vaddr;
    g_client.cli_vaddr = msg->cli_vaddr;
    
    tun_setip(g_client.opt->tundev, inet_ntoa(g_client.cli_vaddr));
    tun_setmask(g_client.opt->tundev, "255.255.255.0");

    epoller_add(g_client.tunfd, &g_client.epoller, client_tun_read, &g_client.linkfd);
    
    uvpn_syslog(LOG_INFO,"client local vaddr %s",inet_ntoa(g_client.cli_vaddr));
    uvpn_syslog(LOG_INFO,"client remote vaddr %s",inet_ntoa(g_client.srv_vaddr));

    /* init the cryptor */
    if (cryptor_init(&g_client.cryptor, AES128_ECB, g_client.opt->passwd, strlen(g_client.opt->passwd)) < 0)
    {
        uvpn_syslog(LOG_ERR,"client init crypto failed");
    }
    
    /* ack */
    link_sendmsg(g_client.linkfd, AUTH_IPASSIGN, msg, sizeof(struct msg_addr));
    
    return 0;
}

static int client_recv_ethdata(void* payload, int len, void* arg)
{
	uint8_t* frame;
	int plen;
    
    /* read plaintext length */
    plen = *(int*)payload;
    uvpn_syslog(LOG_INFO,"client_recv_ethdata %d (%d) ", len, plen);

    /* seek frame pointer to real data */
    frame = (uint8_t*)payload + PRE_DATA_OFFSET;

    debug_packet("eth read predecrypt", (uint8_t*)payload, len);

    /* decrypt */
    if (g_client.cryptor.engine)
    {
        cryptor_decrypt(&g_client.cryptor, frame, len - PRE_DATA_OFFSET, &len);
    
        debug_packet("eth read postdecrypt", (uint8_t*)payload, len);
    }
 
    /* now we write it to server tunfd */
    int wlen = tun_write(g_client.tunfd, frame, len);    

    uvpn_syslog(LOG_INFO,"client write to tundev %d bytes", wlen);
    
    return 0;
}

static int client_recv_karequest(void* payload, int len, void* arg)
{
    /* ack */
    link_sendmsg(g_client.linkfd, KA_RESPONSE, NULL, 0);
     
    uvpn_syslog(LOG_INFO,"client recv keepalive");
    
    return 0;
}

static int client_recv_terminated(void* payload, int len, void* arg)
{
    uvpn_syslog(LOG_INFO,"client recv server is terminated");

    client_terminated(0);

    return 0;
}

static struct msg_handler client_msg_handlers[] = 
{
	{
		.type = AUTH_SHAKEHAND_2,
		.minsize = sizeof(struct msg_shakehand),
		.func = client_recv_shake_2,
	},
	{
		.type = AUTH_CHALLENGE,
		.minsize = CHALLENGE_RESPONSE_SIZE,
		.func = client_recv_challenge,
	},
    {
        .type = AUTH_IPASSIGN,
        .minsize = sizeof(struct msg_addr),
        .func = client_recv_addr,
    },
    {
        .type = ETHERNET_DATA,
        .minsize = 0,
        .func = client_recv_ethdata,
    },
    {
        .type = TERMINATED,
        .minsize = 0,
        .func = client_recv_terminated,
    },
};
#define CLIENT_MSG_HADNLER_ARRAY_LEN (sizeof(client_msg_handlers)/sizeof(client_msg_handlers[0]))


static int client_link_routine(int event, void* handle)
{
	struct epoller_handler* handler = (struct epoller_handler*) handle;
	int fd = handler->fd;
	
	if (0 != (EPOLLIN & event))
	{
		link_recvmsg(fd, NULL);
	}

	if ((0 != (EPOLLERR & event)) || (0 != (EPOLLHUP & event)))
	{
	    uvpn_syslog(LOG_INFO,"client local fd %d closed", fd);
		close(fd);
	}

	return 0;
}

static int connect_establish()
{
	int s = -1;
	if ((s = socket(AF_INET,SOCK_STREAM,0)) < 0)
	{
		uvpn_syslog(LOG_ERR,"Can't create socket. %s(%d)", strerror(errno), errno);
	}

	/* Required when client is forced to bind to specific port */
	int opt=1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 

	struct sockaddr_in srv_addr;
	
	memset(&srv_addr, 0, sizeof(struct sockaddr_in));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(uvpn_opt.bind_addr.port);
	srv_addr.sin_addr.s_addr = inet_addr(uvpn_opt.bind_addr.ip); 
	
	if (connect(s, (struct sockaddr* )&srv_addr, sizeof(srv_addr))< 0)
	{
		uvpn_syslog(LOG_ERR,"Client Connect Error. ", strerror(errno), errno);
		close(s);
		s = -1;
	}

	g_client.linkfd = s;
	
	return s;
}

static void authentication_startup()
{
	struct msg_shakehand msg;

	msg.userid  = g_client.opt->userid;
	msg.authmethod = g_client.opt->authmethod;

	/* send to server */
	link_sendmsg(g_client.linkfd, AUTH_SHAKEHAND_1, &msg, sizeof(msg));

	g_client.state = CLINET_STATE_AUTHSTARTUP;

	return;
}

void uvpn_client(struct cli_opts* cli_opt)
{
	/* init epoller */
	epoller_init(&g_client.epoller);

	g_client.state = CLINET_STATE_INITIAL;

    g_client.opt  = cli_opt;

    /* open tun device */
    g_client.tunfd = tun_open(g_client.opt->tundev);
    if (g_client.tunfd < 0)
    {
        uvpn_syslog(LOG_INFO,"client open tunfd %d", g_client.tunfd);
        return;
    }
    tun_setup(g_client.opt->tundev);
    
	/* register msg handler */
	struct msg_handler* h;
	for (h = client_msg_handlers; h < &client_msg_handlers[CLIENT_MSG_HADNLER_ARRAY_LEN]; ++h)
		msg_handler_register(h);

	/* connect server*/
	if (connect_establish() < 0)
	{
		exit(1);
	}

	authentication_startup();

	epoller_add(g_client.linkfd, &g_client.epoller, client_link_routine, NULL);

	epoller_loop(&g_client.epoller);

	return;
}


