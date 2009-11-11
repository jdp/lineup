#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "message.h"

/*
 * Creates a new message queue in memory.
 * Returns a pointer to the new message queue.
 */
MessageQueue *
MessageQueue_create()
{
	MessageQueue *mq;
	if ((mq = (MessageQueue *)malloc(sizeof(MessageQueue))) == NULL) {
		return NULL;
	}
	mq->heap = NULL;
	mq->capacity = 0;
	mq->size = 0;
	return mq;
}

/*
 * Grows the heap size by a factor of 2
 * Returns 1 on success, 0 on failure
 */
int
MessageQueue_grow(MessageQueue *mq)
{
	Message **new_heap;
	unsigned int new_capacity;
	
	new_capacity = mq->capacity << 1 ? mq->capacity << 1 : 1;
	if ((new_heap = (Message **)malloc(new_capacity * sizeof(Message *))) == NULL) {
		return 0;
	}
	if (mq->heap != NULL) {
		memcpy(new_heap, mq->heap, mq->size * sizeof(Message *));
		free(mq->heap);
	}
	mq->heap = new_heap;
	mq->capacity = new_capacity;
	return 1;
}

/*
 * Creates a new message in memory.
 * Returns a pointer to the message.
 */
Message *
Message_create(int priority, char *message)
{
	Message *m;
	if ((m = (Message *)malloc(sizeof(Message))) == NULL) {
		return NULL;
	}
	m->message = strdup(message);
	m->priority = priority;
	return m;
}

/*
 * Destroys a message in memory.
 */
int
Message_destroy(Message *m)
{
	if (m == NULL) {
		return 0;
	}
	free(m->message);
	free(m);
	return 1;
}

/*
 * Swaps two messages inside the priority queue.
 * When a swap happens, what really happens is the parents and children
 * are swapped.
 */
void
MessageQueue_swap(MessageQueue *mq, unsigned int a, unsigned int b)
{
	Message *tmp;
	tmp = mq->heap[a];
	mq->heap[a] = mq->heap[b];
	mq->heap[b] = tmp;
}

/*
 * Helper functon that ensures the heap stays a min-heap.
 */
void
MessageQueue_bubble_up(MessageQueue *mq, unsigned int k)
{
	int parent;
	/* You can't bubble up the root node */
	if (k == 0) {
		return;
	}
	/* Swap child and parent if parent is higher priority */
	parent = PARENT(k);
	if (SMALLER(mq,k,parent)) {
		MessageQueue_swap(mq, k, parent);
		MessageQueue_bubble_up(mq, parent);
	}
}

/*
 * Helper function that ensures the heap stays a min-heap.
 */
void
MessageQueue_bubble_down(MessageQueue *q, unsigned int k)
{
	int left, right, swap;

	left = LEFT(k);
	right = RIGHT(k);
	
	swap = k;
    if (left < q->size && (PRIORITY(q,left) < PRIORITY(q,k))) {
    	swap = left;
    }
    if (right < q->size && (PRIORITY(q,right) < PRIORITY(q,k))) {
    	if (PRIORITY(q,right) < PRIORITY(q,left)) {
    		swap = right;
    	}
    }
    if (swap == k) {
    	return;
    }
 
    MessageQueue_swap(q, k, swap);
    MessageQueue_bubble_down(q, swap);
}

/*
 * Adds a message to the priority queue, in its sorted position.
 * Returns 1 on success, 0 on failure.
 */
int
MessageQueue_push(MessageQueue *mq, Message *m)
{
	int k;
	
	if (mq->size >= mq->capacity) {
		MessageQueue_grow(mq);
	}
	if (mq->size >= mq->capacity) {
		return 0;
	}
	k = mq->size++;
	mq->heap[k] = m;
	MessageQueue_bubble_up(mq, k);
	return 1;
}

/*
 * Removes and returns the first message in the queue.
 * Returns the first message.
 */
Message *
MessageQueue_pop(MessageQueue *mq)
{
	Message *m;
	
	if (mq->size == 0) {
		return NULL;
	}
	m = mq->heap[0];
	mq->heap[0] = mq->heap[--mq->size];
	if (mq->size > 0) {
		MessageQueue_bubble_down(mq, 0);
	}
	return m;
}

/*
 * Returns the first message in the queue without removing it
 */
Message *
MessageQueue_peek(MessageQueue *mq)
{
	if (mq->size == 0) {
		return NULL;
	}
	return mq->heap[0];
}

/* Debugging function to display the heap */
void
tree(MessageQueue *mq, unsigned int k, int depth, char c)
{
	if (k >= mq->size) {
		return;
	}
	int i;
	for (i = 0; i < depth; i++) {
		printf("  ");
	}
	printf("+%c p'%d m'%s\n", c, mq->heap[k]->priority, mq->heap[k]->message);
	tree(mq, LEFT(k), depth + 1, 'l');
	tree(mq, RIGHT(k), depth + 1, 'r');
}

/*
#define TEST 50
int
main(int argc, char **argv)
{
	MessageQueue *mq;
	if ((mq = MessageQueue_create()) == NULL) {
		printf("couldn't create message queue\n");
		return 1;
	}
	int i;
	srand(time(NULL));
	Message **m = (Message **)malloc(sizeof(Message*) * TEST);
	if (m == NULL) {
		printf("couldn't create %d test messages\n", TEST);
		return 1;
	}
	for (i = 0; i < TEST; i++) {
		m[i] = Message_create(rand()%100, "test");
		MessageQueue_push(mq, m[i]);
	}
	tree(mq, 0, 0, 'R');
	for (i = 0; i < TEST-(TEST/10); i++) {
		MessageQueue_pop(mq);
		tree(mq, 0, 0, 'R');
	}
	return 0;
}
*/
