#ifndef MESSAGE_H
#define MESSAGE_H

#define MAX(a,b) ((a>b)?a:b)
#define PARENT(k) ((k-1)>>1)
#define RIGHT(k) ((k<<1)+1)
#define LEFT(k) ((k<<1)+2)
#define SMALLER(q,k,j) (q->heap[k]->priority <= q->heap[j]->priority)
#define PRIORITY(q,k) (q->heap[k]->priority)

typedef struct message_t
{
	unsigned int id, size;
	int          priority;
	char         *message;
} Message;

typedef struct message_queue_t
{
	Message      **heap;
	unsigned int capacity, size;
} MessageQueue;

MessageQueue* MessageQueue_create(void);
int           MessageQueue_grow(MessageQueue*);
Message*      Message_create(int, char*, size_t);
int           Message_destroy(Message *);
void          MessageQueue_swap(MessageQueue*, unsigned int, unsigned int);
void          MessageQueue_bubble_up(MessageQueue*, unsigned int);
void          MessageQueue_bubble_down(MessageQueue*, unsigned int);
int           MessageQueue_push(MessageQueue*, Message*);
Message*      MessageQueue_pop(MessageQueue*);
Message*      MessageQueue_peek(MessageQueue*);

#endif
