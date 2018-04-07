#ifndef _CONTROL_H_
#define _CONTROL_H_


int cmd_port_open(void);
void cmd_port_close(int fd);

struct cmd_handler{
	struct list_head list;
	int type;
	int minsize;	
	void* (*func)(void* payload, int len, int* slen);
};

void cmd_handler_register(struct cmd_handler* h);
int cmd_routine(int event, void* handle);

#endif