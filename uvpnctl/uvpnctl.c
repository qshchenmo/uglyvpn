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

static void usage(void)
{
    printf("uctl Usage: \n");
    printf("\tuctl command\n");
    printf("\tcommand:\n");
    printf("\tstop  -- stop the uvpnd daemon\n");
    printf("\tshow  -- show uvpnd daemon running status\n");
    printf("\t-h -help -- show usage \n");

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

};

#define COMMAND_ARRAR_LEN (sizeof(commands)/sizeof(commands[0]))

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
   
    /*

    uint8_t rbuf[32] = {0};

    if ((rlen = recvfrom(sock, rbuf, sizeof(rbuf) - 1, 0,  
         (struct sockaddr * )&addr, ( socklen_t * )&addrlen)) < 0 )    
    {  
        uvpn_syslog(LOG_ERR,"uvpnctl recv err");     
        return -1;      
    }  
	*/
    unlink(UVPNCTL_UNIX_PATH); 
    
    close(uvpn_sock);

	return 0;
}