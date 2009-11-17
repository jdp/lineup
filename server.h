#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

typedef struct lineup_server_t
{
	int port;
	int connections;
	MessageQueue *queue;
} LineupServer;

typedef struct lineup_connection_t
{
	LineupServer *server;
	struct evbuffer *inbuf, *outbuf;
	int waiting_on_message, msg_priority, msg_length;
	lineup_connection_t *next;
} LineupConnection;

/* Function prototypes */

int  parser_read_integer(char *, char *, int *);
void server_client_error(LineupServer *, char *);
int  Server_start(int);

#endif
