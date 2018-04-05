#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>

#include "epoller.h"


int epoller_init(struct epoller* self)
{
	int rc = 0;

	self->epfd = epoll_create(1);

	if (self->epfd < 0)
	{
		return -1;
	}

	rc = fcntl(self->epfd, F_SETFD, FD_CLOEXEC);
	self->nevents = 0;
	self->terminate = 0;

	return rc;
}

int epoller_add(int fd, struct epoller* epoller, epoller_callback cb, void* data)
{
	struct epoll_event ev;
	struct epoller_handler* handler = (struct epoller_handler* )malloc(sizeof(struct epoller_handler));

	if (NULL == handler)
	{
		return -1;
	}

	handler->fd = fd;
	handler->cb = cb;
	handler->data = data;

	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN | EPOLLHUP | EPOLLERR;
	ev.data.ptr = (void*)handler;

	if (epoll_ctl(epoller->epfd, EPOLL_CTL_ADD, fd, &ev) < 0)
	{
		return -1;
	}

	return 0;
}

int epoller_del(int fd, struct epoller* epoller)
{
    struct epoll_event ev;

    ev.events = EPOLLIN | EPOLLHUP | EPOLLERR;
    
  	if (epoll_ctl(epoller->epfd, EPOLL_CTL_DEL, fd, &ev) < 0)
	{
		return -1;
	}

	return 0;     
}

void epoller_wait(struct epoller* self, int timeout)
{
	self->nevents = 0;

	while(1)
	{
		self->nevents = epoll_wait(self->epfd, self->events, EPOLLER_MAX_EVENTS, timeout);
		if ((self->nevents == -1) && (errno == EINTR))
			continue;
		break;
	}

	return;
}

void epoller_loop(struct epoller* self)
{
	struct epoller_handler* handler;

	while(!self->terminate)
	{
		epoller_wait(self, -1);
		for (int i = 0; i < self->nevents; ++i)
		{
			handler = self->events[i].data.ptr;
			if (handler->cb)
			{
				handler->cb(self->events[i].events, handler);
			}
		}
	}

	return;
}







