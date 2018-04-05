#ifndef _UGLYVPN_H_
#define _UGLYVPN_H_


#define SADDR_OFFSET 12
#define DADDR_OFFSET 16

#define SADDR(data)   ntohl(*(int*)(data+SADDR_OFFSET))   
#define DADDR(data)   ntohl(*(int*)(data+DADDR_OFFSET))   

unsigned long getifaddrbyname(char* ifname);


int tun_setmask(char* tundev, char* ip);
int tun_setip(char* tundev, char* ip);
int tun_open(char* tundev);
int tun_setup(char* tundev);

int udp_session(int linkfd);

#endif
