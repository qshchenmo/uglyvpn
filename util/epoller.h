#ifndef _EPOLLER_H_
#define _EPOLLER_H_

#include <sys/epoll.h>

#define EPOLLER_MAX_EVENTS 32

typedef int (*epoller_callback)(int event, void* data);


struct epoller
{
	int epfd;
	int nevents;
	int terminate;
	struct epoll_event events[EPOLLER_MAX_EVENTS];
};


struct epoller_handler
{
	int fd;
	epoller_callback cb;
	void* data;
};


int epoller_init(struct epoller* self);
int epoller_add(int fd, struct epoller* epoller, epoller_callback cb, void* data);
int epoller_del(int fd, struct epoller* epoller);
void epoller_wait(struct epoller* self, int timeout);
void epoller_loop(struct epoller* self);

#endif