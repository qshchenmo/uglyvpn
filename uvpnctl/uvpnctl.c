#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <sys/un.h>  

#include "../util/log.h"
#include "uvpnctl.h"


static 	struct sockaddr_un uvpn_addr; 
static  int uvpn_addrlen; 
static  int uvpn_sock;


int cmd_show_recv(void);

static void usage(void)
{
    printf("uctl Usage: \n");
    printf("\tuctl command\n");
    printf("\tcommand:\n");
    printf("\tstop  -- stop the uvpnd daemon\n");
    printf("\tshow  -- show uglyvpn daemon running status\n");
    printf("\thelp  -- show usage \n");

    return;
}

static struct command commands[] = 
{
	{
		.cmd  = "stop",
		.argc = 2,
		.type = CMD_TERMINATE,
		.recv = NULL,
	},
	{
		.cmd  = "show",
		.argc = 2,
		.type = CMD_SHOW_STATUS,
		.recv = cmd_show_recv,
	},
	{
		.cmd  = "help",
		.argc = 2,
		.type = CMD_HELP,
		.recv = NULL,
	},
};

#define COMMAND_ARRAR_LEN (sizeof(commands)/sizeof(commands[0]))

int cmd_show_recv(void)
{
    uint8_t rbuf[1024] = {0};
    int rlen;
    struct cmd_response_status* response;
    struct client_info* client;

    if ((rlen = recvfrom(uvpn_sock, rbuf, sizeof(rbuf) - 1, 0,  
         (struct sockaddr * )&uvpn_addr, ( socklen_t * )&uvpn_addrlen)) < 0 )    
    {  
        return -1;      
    }  

    response = (struct cmd_response_status* )rbuf;
    client = (struct client_info* )((char*)response + sizeof(struct cmd_response_status));

    printf("totol %d connection is active\n", response->num);

    for (int i = 0; i < response->num; ++i)
    {
    	printf("client %d ip %s\n", i, inet_ntoa(client[i].sin_addr));
    	/* code */
    }

    return 0;
}

int cmd_send(int type)
{
	int size = sizeof(struct cmd_head);
	int slen;
	struct cmd_head* cmd_head = (struct cmd_head* )malloc(size);

	cmd_head->type = type;
	cmd_head->len  = 0;
	  
	slen = sendto(uvpn_sock, cmd_head, size, 0, (struct sockaddr* )&uvpn_addr, uvpn_addrlen);

	free(cmd_head);

	return slen;
}

int connect_uvpnd(void)
{
	int sock;
	struct sockaddr_un addr; 
	int addrlen;

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		perror("socket");
		return -1;
	}

	/* bind local address */
	memset(&addr, 0, sizeof(addr));      
    addr.sun_family = AF_UNIX;  
    strncpy(addr.sun_path, UVPNCTL_UNIX_PATH, sizeof(addr.sun_path) - 1 );  
    addrlen = strlen(addr.sun_path ) + sizeof(addr.sun_family); 
    if (bind(sock, ( struct sockaddr * )&addr, addrlen ) < 0 )  
    {    
        perror("bind");
        return -1;
    }

    memset(&uvpn_addr, 0, sizeof(uvpn_addr));    
    uvpn_addr.sun_family = AF_UNIX;
    strncpy(uvpn_addr.sun_path, UVPND_UNIX_PATH, sizeof(uvpn_addr.sun_path) - 1 ); 
    uvpn_addrlen = strlen(uvpn_addr.sun_path ) + sizeof(uvpn_addr.sun_family);

    return sock;
}

int main(int argc, char const *argv[])
{
	int sock;
	struct command* pcommand = NULL;
	int addrlen;

	if (argc < 2)
	{
		usage();
		return 0;
	}

	for (int i = 0; i < COMMAND_ARRAR_LEN; ++i)
	{
		if (!strcmp(argv[1], commands[i].cmd))
		{
			pcommand = &commands[i];
		}
	}

	if ((!pcommand) || (pcommand->argc != argc) || (pcommand->type == CMD_HELP))
	{
		usage();
		return 0;
	}

	uvpn_sock = connect_uvpnd();
	if (uvpn_sock < 0)
	{
		return -1;
	}

    if (cmd_send(pcommand->type) < 0)    
    {  
        uvpn_syslog(LOG_ERR,"uvpnctl write err");      
        return -1;  
    }      
    
    if (pcommand->recv)
    {
    	pcommand->recv();
    }
   
    unlink(UVPNCTL_UNIX_PATH); 
    
    close(uvpn_sock);

	return 0;
}