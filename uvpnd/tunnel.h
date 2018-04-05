#ifndef _TUNNEL_H
#define _TUNNEL_H

struct connector
{
    int rmt_fd;  /* remote */
    int (*encrypt)(char* buf, int len, int* outlen);
    int (*decrypt)(char* buf, int len, int* outlen);
    char buf[FRAME_SIZE];
};


int tunnel_udpread(int event, void* handle);
int tunnel_udpwrite(int fd, char* buf, int len);

int tun_read(int event, void* handle);
int tun_write(int fd, char* buf, int len);



#endif
