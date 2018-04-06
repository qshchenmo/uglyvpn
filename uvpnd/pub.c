#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <syslog.h>
#include <arpa/inet.h>

#include "../util/log.h"
#include "msg.h"

#include "pub.h"

void debug_packet(uint8_t* prefix, uint8_t* frame, int len)
{
	uint8_t strbuf[3*FRAME_SIZE] = {0};
	int wlen = 0;
	int bufsize = 3*FRAME_SIZE;

	snprintf(strbuf + wlen, bufsize - wlen,"%s", prefix);
	wlen = strlen(strbuf);

	for (int i = 0; i < len; ++i)
	{
		snprintf(strbuf + wlen, bufsize - wlen," %x", frame[i]);
		wlen = strlen(strbuf);
	}

    uvpn_syslog(LOG_INFO,"%s", strbuf);
}

void link_recvmsg(int socket, void* arg)
{
	int len;
	struct msg_head* msg_head = NULL;
	struct msg_handler* msg_handler = NULL;
	
	uint8_t buf[FRAME_SIZE];

	buf[FRAME_SIZE-1] = '\0';
	len = recv(socket, buf, FRAME_SIZE, MSG_DONTWAIT);

	if (len > 0)
	{
		msg_head = (struct msg_head*) buf;
		
		msg_handler = msg_handler_lookup(msg_head->type);

		if (msg_handler)
		{
			if (len - sizeof(msg_head) >= msg_handler->minsize)
				msg_handler->func((void*)((unsigned char*)msg_head + sizeof(struct msg_head)), len - sizeof(msg_head), arg);
		}
	}

	return;
}

void link_sendmsg(int socket, int type, void* payload, int len)
{
	int size = sizeof(struct msg_head) + len;
	
	struct msg_head* msg_head = (struct msg_head* )malloc(size);

	msg_head->type = type;
	msg_head->len  = len;

    if (payload)
	{
        memcpy((void*)&msg_head->payload[0], payload, len);
    }    
	
	int l = send(socket, msg_head, size, 0);
	if (l < 0)
	{
		uvpn_syslog(LOG_INFO,"link send failed", strerror(errno), errno);
	}

	free(msg_head);

	return;
}


