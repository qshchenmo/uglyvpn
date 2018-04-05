
#include "list.h"

#define LIST_POISON1  ((void *) 0x00100100)
#define LIST_POISON2  ((void *) 0x00200200)

static void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	if ((next->prev != prev)) {
		// error
	}
	if ((prev->next != next)) {
		// error
	}
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

static void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}



