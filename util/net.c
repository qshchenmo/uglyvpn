#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h> 
#include <errno.h>
#include <syslog.h>



#include "../util/log.h"


/* Get interface address */
unsigned long getifaddrbyname(char* ifname) 
{
    struct ifreq ifr;
    int s;

    if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
    	return -1;
    }
        
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name)-1);
    ifr.ifr_name[sizeof(ifr.ifr_name)-1]='\0';

    if( ioctl(s, SIOCGIFADDR, &ifr) < 0){
        close(s);
        return -1;
    }
    close(s);

    return ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr;
}


/* set tun device's addr */
static int tun_setaddr(char* tundev, char* ip, int flag)
{
    struct ifreq ifr;
    struct sockaddr_in sin;
    int sockfd;
    int err;

    memset(&ifr, 0, sizeof(ifr));

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        uvpn_syslog(LOG_ERR,"Create Query fd Failed");	
        return -1;
    }
        
    sin.sin_family = AF_INET;
    inet_aton(ip, &sin.sin_addr);

    snprintf(ifr.ifr_name, (sizeof(ifr.ifr_name) - 1), "%s", tundev);
    
    memcpy(&ifr.ifr_addr, &sin, sizeof(struct sockaddr));
    if((err = ioctl(sockfd, flag, (void *)&ifr)) < 0 ) {
        uvpn_syslog(LOG_ERR,"ioctl setaddr Failed");	
    }

    close(sockfd);

    return err;
}


int tun_setup(char* tundev)
{
    struct ifreq ifr;
    int sockfd;
    int err;

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, (sizeof(ifr.ifr_name) - 1), "%s", tundev);
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        uvpn_syslog(LOG_ERR,"Create Query fd Failed");	
        return -1;
    }
        
    if((err = ioctl(sockfd, SIOCGIFFLAGS, (void *)&ifr)) < 0 ) 
    {
        uvpn_syslog(LOG_ERR,"ioctl get ifflag Failed err %d", err);	
    }

    ifr.ifr_flags |= IFF_UP;
    if((err = ioctl(sockfd, SIOCSIFFLAGS, (void *)&ifr)) < 0 ) 
    {
        uvpn_syslog(LOG_ERR,"ioctl set ifflag Failed err %d", err); 
    }

    close(sockfd);

    return err;
}

int tun_setmask(char* tundev, char* ip)
{
    return tun_setaddr(tundev, ip, SIOCSIFNETMASK);
}

int tun_setip(char* tundev, char* ip)
{
    return tun_setaddr(tundev, ip, SIOCSIFADDR);
}

/* open tun device  */
int tun_open(char *tundev) 
{
    struct ifreq ifr;
    int fd, err;
    char *tun = "/dev/net/tun";

    if((fd = open(tun , O_RDWR)) < 0 ) 
    {
        uvpn_syslog(LOG_ERR,"opening /dev/net/tun failed");	
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = IFF_NO_PI | IFF_TUN;

    strncpy(ifr.ifr_name, tundev, IFNAMSIZ);

    if((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 ) {
        uvpn_syslog(LOG_ERR,"ioCTL TUNSETIFF Failed");	
	    close(fd);
        return err;
    }

    return fd;
}


int udp_session(int linkfd)
{
    struct sockaddr_in addr;
    int s, opt, socklen;

    /* create dgram socket */
    if((s = socket(AF_INET,SOCK_DGRAM,0)) < 0){
        uvpn_syslog(LOG_ERR,"Can't create socket");
        return -1;
    }
    
    /* get remote address */
    socklen = sizeof(struct sockaddr_in);
    if (getsockname(linkfd, (struct sockaddr *)&addr, &socklen) < 0)
    {
       uvpn_syslog(LOG_ERR,"Can't get local name");
       goto failed;
    }

    uvpn_syslog(LOG_INFO,"[session]bind addr ip %s port port %d", inet_ntoa(addr.sin_addr), addr.sin_port);

    opt=1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 

    if(bind(s,(struct sockaddr *)&addr,sizeof(addr)) ){
       uvpn_syslog(LOG_ERR,"Can't bind to the socket");
       goto failed;
    }

    socklen = sizeof(struct sockaddr_in);
    if(getpeername(linkfd,(struct sockaddr *)&addr,&socklen) ){
       uvpn_syslog(LOG_ERR,"Can't get peer name");
       goto failed;
    }

    uvpn_syslog(LOG_INFO,"[session]connect addr ip %s port port %d", inet_ntoa(addr.sin_addr), addr.sin_port);
    if(connect(s,(struct sockaddr *)&addr,sizeof(addr)) ){
        uvpn_syslog(LOG_ERR,"Can't connect socket");
        goto failed;
    }

    return s;
    
failed:
    close(s);

    return -1;

}

