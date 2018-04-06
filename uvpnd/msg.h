#ifndef _MSG_H_
#define _MSG_H_

#include "../util/list.h"

struct ipcmsg{
    int type;
};

struct msg_head{
	int type;
	int len;
	void* payload[0];
};

struct msg_handler{
	struct list_head list;
	int type;
	int minsize;	
	int (*func)(void* payload, int len, void* arg);
};

void msg_handler_register(struct msg_handler* h);
struct msg_handler* msg_handler_lookup(int type);


#endif
