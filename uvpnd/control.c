#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <sys/un.h>  


#include "../util/list.h"
#include "../util/net.h"
#include "../util/log.h"
#include "../util/epoller.h"

#include "../uvpnctl/uvpnctl.h"
#include "control.h"


int cmd_port_open(void)
{
	int sock;
	struct sockaddr_un addr;
	int addrlen;

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if(sock == -1){
        uvpn_syslog(LOG_ERR,"open uvpnctl socket failed");   
        return -1;
    }

    unlink(UVPND_UNIX_PATH);

    memset(&addr, 0, sizeof(addr));   
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, UVPND_UNIX_PATH, sizeof(addr.sun_path ) - 1 );      
    
    addrlen = strlen(addr.sun_path) + sizeof(addr.sun_family);
    if (bind(sock, (struct sockaddr*)&addr, addrlen) < 0)
   	{
   		uvpn_syslog(LOG_ERR,"bind uvpnctl socket failed");   
		close(sock);
		return -1;   		
   	}

   	uvpn_syslog(LOG_INFO,"uvpn control socket %d open", sock);  

   	return sock;
}

void cmd_port_close(int fd)
{
	close(fd);

    unlink(UVPND_UNIX_PATH);

    return;
}


static struct list_head cmdsw = {&cmdsw, &cmdsw};

void cmd_handler_register(struct cmd_handler* c)
{
	struct list_head* item;
	struct list_head* last_item;
	struct cmd_handler* answer;

	last_item = &cmdsw;
	list_for_each(item, &cmdsw){
		answer = list_entry(item, struct cmd_handler, list);
		if (c->type == answer->type)
			goto out;
		last_item = item;
	}

	list_add(&c->list, last_item);
out:
	return;
}

struct cmd_handler* cmd_handler_lookup(int type)
{
	struct cmd_handler* answer = NULL;
	struct list_head* item;
	
	list_for_each(item, &cmdsw){
		answer = list_entry(item, struct cmd_handler, list);
		if (type == answer->type)
			return answer;
	}

	return NULL;
}

int cmd_routine(int event, void* handle)
{
	struct sockaddr_un addr;
	int addrlen = sizeof(addr);
	struct epoller_handler* handler = (struct epoller_handler* )handle;
	uint8_t rbuf[128];
	uint8_t sbuf[128];
	int rlen, slen;
	struct cmd_head* cmd_head = NULL;
	struct cmd_handler* cmd_handler = NULL;
	
	rlen = recvfrom(handler->fd, rbuf, sizeof(rbuf) - 1, 0,    
                    (struct sockaddr * )&addr,(socklen_t *)&addrlen);    
    
    if (rlen < 0 )    
    {    
        uvpn_syslog(LOG_ERR, "control cmd receive error"); 
        return -1;    
    }    

    cmd_head = (struct cmd_head*)rbuf;		
	cmd_handler = cmd_handler_lookup(cmd_head->type);

	if (cmd_handler)
	{
		if (rlen - sizeof(cmd_head) >= cmd_handler->minsize)
			cmd_handler->func((void*)((unsigned char*)cmd_head + sizeof(struct cmd_head)), rlen - sizeof(cmd_head), sbuf, &slen);
	
		if (sendto(handler->fd, sbuf, slen, 0, (struct sockaddr *)&addr, addrlen) < 0 )    
    	{    
        	uvpn_syslog(LOG_ERR, "control cmd response error"); 
        	return -1;    
    	}  
	}

    return 0;
}
