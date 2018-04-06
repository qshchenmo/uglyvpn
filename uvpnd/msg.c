#include "../util/list.h"
#include "msg.h"

static struct list_head msgsw = {&msgsw, &msgsw};

void msg_handler_register(struct msg_handler* h)
{
	struct list_head* item;
	struct list_head* last_item;
	struct msg_handler* answer;

	last_item = &msgsw;
	list_for_each(item, &msgsw){
		answer = list_entry(item, struct msg_handler, list);
		if (h->type == answer->type)
			goto out;
		last_item = item;
	}

	list_add(&h->list, last_item);
out:
	return;
}


struct msg_handler* msg_handler_lookup(int type)
{
	struct msg_handler* answer = NULL;
	struct list_head* item;
	
	list_for_each(item, &msgsw){
		answer = list_entry(item, struct msg_handler, list);
		if (type == answer->type)
			return answer;
	}

	return NULL;
}

