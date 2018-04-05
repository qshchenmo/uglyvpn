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
#include "../util/msg.h"
#include "../util/random.h"
#include "../util/timer.h"
#include "../util/crypto.h"

#include "pub.h"
#include "tunnel.h"
#include "user.h"
#include "uvpnd.h"
#include "server.h"

extern struct uvpn_opts uvpn_opt;

#define AUTH_MAX_PROCESS 16	
#define PROCESS_STATE_INITIAL       0 
#define PROCESS_STATE_RECVSHAKE_1   1
#define PROCESS_STATE_CHANLLENGE    2
#define PROCESS_STATE_SESSION       3

struct process{
	int state;
	int linkfd;                  /* stream socket connection with client */
	int index;
    char* cli_ip;                /* client ip */
    struct in_addr cli_vaddr;    /* clinet virtual address */
	struct user* user;
    int kacnt;
    int kafd;                    /* keep alive fd            */
 //   int (*encrypt)(char* buf, int len, int* outlen);
 //   int (*decrypt)(char* buf, int len, int* outlen);
    struct cryptor cryptor;
};

struct uvpn_server_run{
	struct srv_opts* opt;
    int tunfd;
	struct epoller epoller;
	struct process** process;
	uint16_t n_used;
	uint16_t* unused;
};

struct uvpn_server_run g_server;

static struct user user1 = {.userid = 123,
							.sp_authmethod = AUTH_METHOD_PASSWORD,
							.ef_authmethod = 0,
							.passwd = "test",
						    };

static void server_ipassign(struct process* p)
{
    p->state = PROCESS_STATE_SESSION;

    struct msg_addr msg;

    inet_aton(g_server.opt->srv_ip, &msg.srv_vaddr);
    inet_aton("10.100.0.101", &msg.cli_vaddr);
    
    uvpn_syslog(LOG_INFO,"server ip %s", g_server.opt->srv_ip);   

    /* send to client */
	link_sendmsg(p->linkfd, AUTH_IPASSIGN, &msg, sizeof(msg));

    return;
}

static void server_send_challenge(struct process* p)
{
	// struct msg_chanresp chanresp;

	/* set new state */
	p->state = PROCESS_STATE_CHANLLENGE;

	/* generate challenge-response */
	random_generator(p->user->chanresp, CHALLENGE_RESPONSE_SIZE);

	//memcpy(chanresp.chanresp, p->user->chanresp, CHALLENGE_RESPONSE_SIZE);

	/* send to client */
	link_sendmsg(p->linkfd, AUTH_CHALLENGE, p->user->chanresp, CHALLENGE_RESPONSE_SIZE);
	
	return;
}

static int server_recv_response(void* payload, int len, void* arg)
{
	struct process* p = (struct process* )arg;
	uint8_t* response = (uint8_t* )payload;
	uint8_t challenge[CRYPTO_MAX_SIZE];
	uint32_t outlen;

	if (p->state != PROCESS_STATE_CHANLLENGE)
	{
		return -1;
	}

    /* update ka timer */
    p->kacnt = 0;
    
    simple_decrypt(response, challenge, len, p->user->passwd, strlen(p->user->passwd), &outlen);

/*
	ecb_decrypt(msg->chanresp, msg->chanresp, 
				CHALLENGE_RESPONSE_SIZE, 
				p->user->passwd, 
				strlen(p->user->passwd));
*/

	if (!memcmp(challenge, p->user->chanresp, CHALLENGE_RESPONSE_SIZE))
	{
		uvpn_syslog(LOG_INFO,"server auth user %d success", p->user->userid);	

		/* allocate IP for client */
        server_ipassign(p);
	}
	else
	{
		uvpn_syslog(LOG_INFO,"server auth user %d failed", p->user->userid);
	}
	return 0;
}

static void process_terminate(struct process* p)
{
	int index = p->index;

	uvpn_syslog(LOG_INFO,"process %d teminated",p->linkfd);

	g_server.process[index] = NULL;
	--g_server.n_used;

    if (p->kafd)
    {
        close(p->kafd);
    }
    
	close(p->linkfd);
	free(p);

	return;
}

static int server_recv_shake_1(void* payload, int len, void* arg)
{
	struct process* p = (struct process* )arg;
	struct msg_shakehand* msg = (struct msg_shakehand* )payload;
    struct sockaddr_in cli_addr;
    int socklen;
    
	if (p->state != PROCESS_STATE_INITIAL)
	{
		return -1;
	}

    /* update ka timer */
    p->kacnt = 0;

    /* get client address  */
    socklen = sizeof(struct sockaddr_in);
    if (getpeername(p->linkfd, (struct sockaddr*)&cli_addr, &socklen))
    {
        uvpn_syslog(LOG_INFO,"Get peer name failed");
        return -1;
    }

    p->cli_ip = strdup(inet_ntoa(cli_addr.sin_addr));
    uvpn_syslog(LOG_INFO,"server receive connect from %s userid = %d opt=%d", 
               p->cli_ip,
               msg->userid, 
               msg->authmethod);
    
	/* TODO find user 1 */
	p->user = &user1;	
	
	struct msg_shakehand ack;
	ack.userid  = p->user->userid;
	ack.authmethod = AUTH_METHOD_PASSWORD;

	p->user->ef_authmethod = AUTH_METHOD_PASSWORD;
	p->state  = PROCESS_STATE_RECVSHAKE_1;

	link_sendmsg(p->linkfd, AUTH_SHAKEHAND_2, &ack, sizeof(ack));
	
	return 0;
}

static int server_recv_shake_3(void* payload, int len, void* arg)
{
	struct process* p = (struct process* )arg;
	struct msg_shakehand* msg = (struct msg_shakehand* )payload;

	if (p->state != PROCESS_STATE_RECVSHAKE_1)
	{
		return -1;
	}

    /* update ka timer */
    p->kacnt = 0;
        
	uvpn_syslog(LOG_INFO,"server receive AUTH_SHAKEHAND_3 userid = %d opt=%d", msg->userid, msg->authmethod);

	if (p->user->userid != msg->userid || 
		p->user->ef_authmethod != msg->authmethod)
	{
	    uvpn_syslog(LOG_INFO,"server receive userid %d authmethod %d, but user id %d authmethod %d is expected ",
                    msg->userid, msg->authmethod, p->user->userid, p->user->ef_authmethod);
		process_terminate(p);
		return -1;
	}

	if (p->user->ef_authmethod == AUTH_METHOD_PASSWORD)
	{
        server_send_challenge(p);
	}
	else if (p->user->ef_authmethod == AUTH_METHOD_CERTIFICATE)
	{
		// TODO
	}
	
	return 0;
}

static int server_recv_addr(void* payload, int len, void* arg)
{
    struct process* p = (struct process* )arg;

    /* update ka timer */
    p->kacnt = 0;

    /* init the */
    if (cryptor_init(&p->cryptor, AES128_ECB, p->user->passwd, strlen(p->user->passwd)) < 0)
    {
        uvpn_syslog(LOG_ERR,"process %d init crypto failed", p->index);
    }
    
    return 0;
}


/* receive ethernet data */
static int server_recv_ethdata(void* payload, int len, void* arg)
{
    struct process* p = (struct process* )arg;
    uint8_t* frame;
    int plen;

    /* update ka timer */
    p->kacnt = 0;

    /* read plaintext length */
    plen = *(int*)payload;
    uvpn_syslog(LOG_INFO,"server_recv_ethdata %d (%d) ", len, plen);
    
    /* seek frame pointer to real data */
    frame = (uint8_t*)payload + PRE_DATA_OFFSET;

    debug_packet("eth recv predecrypt", (uint8_t*)payload, len);

    /* decrypt */
    if (p->cryptor.engine)
    {
        cryptor_decrypt(&p->cryptor, frame, len - PRE_DATA_OFFSET, &len);
    }
 
 	debug_packet("eth recv postdecrypt", (uint8_t*)payload, len);

    /* TODO:  extract virtual destination addr in IP frame header */
    struct in_addr dip;
    dip.s_addr = DADDR(frame);

    uvpn_syslog(LOG_INFO,"server receive %d bytes from %s", len, inet_ntoa(dip));

    /* now we write it to server tunfd */
    int wlen = tun_write(g_server.tunfd, frame, plen);    

    uvpn_syslog(LOG_INFO,"server write to tundev %d bytes", wlen);
    
    return 0;
}

static int server_recv_karesponse(void* payload, int len, void* arg)
{
    struct process* p = (struct process* )arg;

    p->kacnt = 0;

    uvpn_syslog(LOG_INFO,"server recv keepalive response");
    
    return 0;
}

static struct msg_handler server_msg_handlers[] = 
{
	{
		.type = AUTH_SHAKEHAND_1,
		.minsize = sizeof(struct msg_shakehand),
		.func = server_recv_shake_1,
	},
	{
		.type = AUTH_SHAKEHAND_3,
		.minsize = sizeof(struct msg_shakehand),
		.func = server_recv_shake_3,
	},
	{
		.type = AUTH_RESPONSE,
		.minsize = CHALLENGE_RESPONSE_SIZE,
		.func = server_recv_response,
	},
	{
        .type = AUTH_IPASSIGN,
        .minsize = sizeof(struct msg_addr),
        .func = server_recv_addr,
	},
    {
        .type = ETHERNET_DATA,
        .minsize = 0,
        .func = server_recv_ethdata,
    },
    {
        .type = KA_RESPONSE,
        .minsize = 0,
        .func = server_recv_karesponse,
    },
};

#define SERVER_MSG_HADNLER_ARRAY_LEN (sizeof(server_msg_handlers)/sizeof(server_msg_handlers[0]))

int server_link_routine(int event, void* handle)
{
	struct process* p;

	struct epoller_handler* handler = (struct epoller_handler*) handle;

	p = (struct process *)handler->data;
	
	if (0 != (EPOLLIN & event))
	{
		link_recvmsg(p->linkfd, p);
	}

    
	if ((0 != (EPOLLERR & event)) || (0 != (EPOLLHUP & event)))
	{
	    uvpn_syslog(LOG_INFO,"server process %d reveive EPOLLHUP or EPOLLHUP", p->index);
            
		process_terminate(p);
	}


	return 0;
}

int process_create(int client_fd, struct sockaddr_in* client)
{
	struct process *p = NULL;

	int index;

	p = (struct process* )malloc(sizeof(struct process));
	p->state  = PROCESS_STATE_INITIAL;
	p->linkfd = client_fd;
    p->kacnt = 0;
    p->kafd   = -1;
    
	index = g_server.unused[AUTH_MAX_PROCESS - g_server.n_used -1];
	g_server.process[index] = p;
	p->index = index;

	++g_server.n_used;

    if ((p->kafd = uvpn_timer_create(KA_INTERVAL)) < 0)
    {
        uvpn_syslog(LOG_ERR,"process %d ka timer create failed", p->index);
    }
    
	uvpn_syslog(LOG_INFO,"process %d create",p->index);

	return index;
}


/* server send keep alive */
int server_katimer_routine(int event, void* handle)
{
    struct process* p;
	struct epoller_handler* handler = (struct epoller_handler*) handle;
    uint64_t value;
    
	p = (struct process *)handler->data;

	if (0 != (EPOLLIN & event))
	{
	    p->kacnt++;
	    read(p->kafd, &value, sizeof(uint64_t));
        if(p->kacnt == KA_EXPIRE_CNT)
        {
            uvpn_syslog(LOG_INFO,"server process %d send keepalive dgram", p->index);
            link_sendmsg(p->linkfd, KA_REQUEST, NULL, 0); 
        }
        else if (p->kacnt > KA_EXPIRE_CNT)
        {
            uvpn_syslog(LOG_INFO,"server process %d keepalive timeout", p->index);
            process_terminate(p);
        }
	}

	return 0;
}

void access_client(int client_fd, struct sockaddr_in* client)
{
	int index;
	index = process_create(client_fd, client);

	epoller_add(client_fd, &g_server.epoller, server_link_routine, g_server.process[index]);
    epoller_add(g_server.process[index]->kafd, &g_server.epoller, server_katimer_routine, g_server.process[index]);
    
	return;
}

static int connect_request(int event, void* handle)
{
	struct epoller_handler* handler = (struct epoller_handler* )handle;
	struct sockaddr_in client_addr;
	int client_len;
	int client_fd;

	assert(0 != (EPOLLIN & event));

 	client_len = sizeof(client_addr);

 	if ((client_fd = accept(handler->fd, (struct sockaddr*)&client_addr, &client_len)) < 0) 
 	{
 		uvpn_syslog(LOG_INFO, "accept socket failed");
 	}

    access_client(client_fd, &client_addr);
    
 	return 0;
}

static int server_port_open()
{
	struct sockaddr_in local, client;
	int s = -1;

	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;

	local.sin_addr.s_addr = getifaddrbyname(uvpn_opt.bind_addr.name);
	if (uvpn_opt.bind_addr.port)
	{
		local.sin_port = htons(uvpn_opt.bind_addr.port);
	}

 	if((s=socket(AF_INET,SOCK_STREAM,0)) < 0){
		uvpn_syslog(LOG_ERR,"Can't create socket");
		exit(1);
    }

    int opt=1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 
    setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    if(bind(s,(struct sockaddr *)&local,sizeof(local)) ){
		uvpn_syslog(LOG_ERR,"Can't bind to the socket");
		exit(1);
    }

    if(listen(s, 10) ){
		uvpn_syslog(LOG_ERR,"Can't listen on the socket");
		exit(1);
    }

    return s;
}

static int write_pidfile(void)
{
	int fd;
	char buf[32];
	struct flock region;
	
	fd = open(UVPN_DEFAULT_PID_PATH, O_RDWR|O_CREAT, 0644);
	if (fd < 0) {
		return -1;
	}	

 	region.l_type = F_WRLCK;  
	region.l_whence = SEEK_SET;  
  
	if (fcntl(fd, F_SETLK, &region) < 0){  
    	close(fd);
		return -1;
	}  

	ftruncate(fd, 0);  

	snprintf(buf, 32, "%ld", (long)getpid());

	write(fd, buf, strlen(buf)+1);

	return 0;
}

static int _server_tun_read(int tunfd)
{
	int len;
    uint8_t buf[FRAME_SIZE];
    uint8_t* frame;
    struct process* p;
        
    /* read from tundev , put it in &buf[PRE_DATA_OFFSET] */    
	len = read(tunfd, &buf[PRE_DATA_OFFSET], sizeof(buf));
    if (len < 0)
    {
         uvpn_syslog(LOG_ERR,"tunread failed on tunfd %d", tunfd);
         return -1;
    }

    frame = &buf[PRE_DATA_OFFSET];

    /* extract virtual destination addr in IP frame header */
    struct in_addr dip;
    dip.s_addr = DADDR(frame);

   	debug_packet("tun read preencrypt", (uint8_t*)buf, len + PRE_DATA_OFFSET);

    /* TODO find process */
    p = g_server.process[0];
    
    if (p && p->linkfd > 0)
    {
    	*((int*)&buf[0]) = len;
        if (p->cryptor.engine)
        {
            cryptor_encrypt(&p->cryptor, frame, len, &len);
        }

        debug_packet("tun read postencrypt", (uint8_t*)buf, len + PRE_DATA_OFFSET);

        link_sendmsg(p->linkfd, ETHERNET_DATA, buf, len + PRE_DATA_OFFSET);

        uvpn_syslog(LOG_INFO,"tunrecv %d bytes and send to peer", len + PRE_DATA_OFFSET);
    }
       
    return 0;
}

int server_tun_read(int event, void* handle)
{
	struct epoller_handler* handler = (struct epoller_handler*) handle;
	int tunfd  = handler->fd;

	if (0 != (EPOLLIN & event))
	{
		_server_tun_read(tunfd);
	}

	if ((0 != (EPOLLERR & event)) || (0 != (EPOLLHUP & event)))
	{
	 //   uvpn_syslog(LOG_INFO,"client local fd %d closed", fd);
		//close(fd);
	}

	return 0;
}


void uvpn_server(struct srv_opts* opt)
{	
	int s;

	g_server.opt = opt;

	if (write_pidfile() < 0)
	{
		uvpn_syslog(LOG_ERR,"Open PID file Failed, Exit");
		exit(1);
	}

	epoller_init(&g_server.epoller);

	s = server_port_open();
	epoller_add(s, &g_server.epoller, connect_request, NULL);

    /* open tun fd */
    g_server.tunfd = tun_open(g_server.opt->tundev);
    if (g_server.tunfd  < 0)
    {
        uvpn_syslog(LOG_INFO,"server open tunfd %d failed", g_server.tunfd);
        return;
    }
    tun_setip(g_server.opt->tundev, g_server.opt->srv_ip);
    tun_setmask(g_server.opt->tundev, "255.255.255.0");
    tun_setup(g_server.opt->tundev);
        
    epoller_add(g_server.tunfd, &g_server.epoller, server_tun_read, NULL);

	/* init multi process  for client */
	g_server.process = malloc(sizeof(struct process*) * AUTH_MAX_PROCESS + sizeof(uint16_t) * AUTH_MAX_PROCESS);
	g_server.unused = (uint16_t*) (g_server.process + AUTH_MAX_PROCESS);
	for (int i = 0; i != AUTH_MAX_PROCESS; ++i)
	{
		g_server.unused[i] = AUTH_MAX_PROCESS - i - 1;
	}

	/* register message handler */
	struct msg_handler* h;
	for (h = server_msg_handlers; h < &server_msg_handlers[SERVER_MSG_HADNLER_ARRAY_LEN]; ++h)
		msg_handler_register(h);
	
	uvpn_syslog(LOG_INFO, "ugly-vpnd server running!");

	/* epoll loop */
	epoller_loop(&g_server.epoller);

	return;
}
