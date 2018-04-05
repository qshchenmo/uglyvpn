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

#include "pub.h"

#include "tunnel.h"

static int tunnel_udprecv(int linkfd, int tunfd)
{
    	
	uint8_t buf[128];

	buf[128-1] = '\0';
	int len = read(linkfd, buf, 128);

    if (len > 0)
    {
        tun_write(tunfd, buf, len);
        uvpn_syslog(LOG_INFO,"recv %s", buf);
    }

    return 0;
}

int tunnel_udpwrite(int fd, char* buf, int len)
{
    int wlen = 0;
 
	if((wlen = write(fd, buf, len)) < 0 ){ 
	      return 0;
	}

    return wlen;
}

int tunnel_udpread(int event, void* handle)
{
	struct epoller_handler* handler = (struct epoller_handler*) handle;
	int linkfd = handler->fd;
	int tunfd = *(int*)handler->data;
    
	if (0 != (EPOLLIN & event))
	{
		tunnel_udprecv(linkfd, tunfd);
	}

    /*
	if ((0 != (EPOLLERR & event)) || (0 != (EPOLLHUP & event)))
	{
	    uvpn_syslog(LOG_INFO,"client local fd %d closed", fd);
		close(linkfd);
	}
      */
	return 0;
}

int tun_write(int tunfd, char* buf, int len)
{
    int wlen = 0;
	if((wlen = write(tunfd, buf, len)) < 0 ){ 
	      return 0;
	}

    return wlen;
}

/*
static int _tun_read(int tunfd, struct connector* connector)
{
	int len;

	len = read(tunfd, connector->buf, sizeof(connector->buf));
    if (len < 0)
    {
         uvpn_syslog(LOG_ERR,"tunread failed on tunfd %d", tunfd);
         return -1;
    }

    if (connector->encrypt)
    {
        connector->encrypt(connector->buf, len, &len);
    }

    link_sendmsg(connector->rmt_fd, ETHERNET_DATA, connector->buf, len);

    uvpn_syslog(LOG_INFO,"tunrecv %d bytes and send to peer", len);
   
    return 0;
}


int tun_read(int event, void* handle)
{
	struct epoller_handler* handler = (struct epoller_handler*) handle;
	int tunfd  = handler->fd;
    //int linkfd = *(int*)handler->data;

    struct connector* connector = (struct connector* )handler->data;
    
	if (0 != (EPOLLIN & event))
	{
		_tun_read(tunfd, connector);
	}

	if ((0 != (EPOLLERR & event)) || (0 != (EPOLLHUP & event)))
	{
	 //   uvpn_syslog(LOG_INFO,"client local fd %d closed", fd);
		//close(fd);
	}

	return 0;
}
*/


