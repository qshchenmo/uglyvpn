#ifndef _PUB_H_
#define _PUB_H_

#include <netinet/in.h>


#define FRAME_SIZE          1024

#define PRE_DATA_OFFSET        4
#define KA_INTERVAL            10
#define KA_EXPIRE_CNT          3   

#define AUTH_METHOD_PASSWORD      0x01
#define AUTH_METHOD_CERTIFICATE   0x02


#define PASSWORD_MAXSIZE         16
#define CHALLENGE_RESPONSE_SIZE  16

/* Message defination between client & server */
#define AUTH_SHAKEHAND_1  1
#define AUTH_SHAKEHAND_2  2
#define AUTH_SHAKEHAND_3  3
#define AUTH_CHALLENGE    4
#define AUTH_RESPONSE     5
#define AUTH_IPASSIGN     6
#define ETHERNET_DATA     7
#define KA_REQUEST        8
#define KA_RESPONSE       9
#define TERMINATED      100


struct msg_shakehand{
	uint32_t userid;
	uint8_t  authmethod;
};

struct msg_addr{
    struct in_addr srv_vaddr;
    struct in_addr cli_vaddr;
};

struct msg_tundata
{
    int len;
    char* data;
};

void debug_packet(uint8_t* prefix, uint8_t* frame, int len);
void link_recvmsg(int socket, void* arg);
void link_sendmsg(int socket, int type, void* payload, int len);


#endif
