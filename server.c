#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "message.h"
#include "server.h"

LineupConnection *connections;

/*
 * Reads an integer into *result, from the source string *src. It returns
 * the number of bytes read while getting the integer.
 */
int
parser_read_integer(char *src, char *stop, int *result)
{
	size_t bytes_read;
	*result = 0;
	bytes_read = 0;
	if (isdigit(*(src + bytes_read))) {
		while (isdigit(*(src + bytes_read)) && ((src + bytes_read) < stop)) {
			*result = (*result * 10) + (*(src + bytes_read) - '0');
			bytes_read++;
		}
	}
	return bytes_read;
}

/*
 * Sends an error message to the client. It also forces the server to
 * stop waiting on a message body. Finally, it drains the input buffer
 * so that the next call to server_read_cb starts clean.
 */
void
server_client_error(LineupServer *server, char *error)
{
	server->waiting_on_message = 0;
	evbuffer_add_printf(server->outbuf, "-%s\r\n", error);
	evbuffer_drain(server->inbuf, evbuffer_get_length(server->inbuf));
}

static void
server_read_cb_dummy(struct bufferevent *bev, void *ctx)
{
	LineupServer *server = (LineupServer *)ctx;
	server->inbuf = bufferevent_get_input(bev);
	size_t len = evbuffer_get_length(server->inbuf);
	char *data;
	printf("sup\n");
	data = (char *)malloc(sizeof(char)*len);
	bufferevent_read(bev, data, len);
	printf("current input: %s\n", data);
	free(data);
	//bufferevent_free(bev);
}

/*
 * Reacts to read events on the input buffer. This is the dispatching
 * function and will react to commands received on the server.
 */
static void
server_read_cb(struct bufferevent *bev, void *ctx)
{
	size_t len;
	LineupServer *server;
	Message *m;
	char *tmp, *request_line, *parser, *message_body;
	int priority, message_length, check;
	
	server = (LineupServer *)ctx;
	server->inbuf = bufferevent_get_input(bev);
	server->outbuf = bufferevent_get_output(bev);
	len = evbuffer_get_length(server->inbuf);
	
	printf("recv'd size: %d\n", len);
	
	/* Since we're not waiting on a message, look for commands */
	if (!server->waiting_on_message) {
	
		/* Look for commands */
		request_line = evbuffer_readln(server->inbuf, &len, EVBUFFER_EOL_CRLF);
		parser = request_line;
		
		printf("recv'd: %s\n", request_line);
		
		if (request_line) {
	
			/* Respond to the EXIT command */
			if (!strncmp(request_line, "EXIT", 4)) {
				bufferevent_free(bev);
			}
		
			/* Respond to the PUSH command */
			else if (!strncmp(request_line, "PUSH ", 5)) {
				/* Read priority */
				parser += 5;
				if (isdigit(*parser)) {
					parser += parser_read_integer(parser, request_line + len, &priority);
				}
				else {
					server_client_error(server, "EXPECTED_PRIORITY");
					return;
				}
				/* Read message length */
				if (!isspace(*parser)) {
					server_client_error(server, "EXPECTED_MESSAGE_LENGTH");
					return;
				}
				parser++;
				if (isdigit(*parser)) {
					parser += parser_read_integer(parser, request_line + len, &message_length);
				}
				else {
					server_client_error(server, "EXPECTED_MESSAGE_LENGTH");
					return;
				}
				/*
				if ((*(parser + 1) != '\r') && (*(parser + 2) != '\n')) {
					printf("\\%x\\%x\n", *(parser+1), *(parser + 2));
					server_client_error(server, "EXPECTED_TERMINATOR");
					return;
				}
				*/
				/* Next block of data should be a message body */
				server->msg_length = message_length;
				server->msg_priority = priority;
				server->waiting_on_message = 1;
			}
			
			/* Respond to POP command */
			else if (!strncmp(request_line, "POP", 3)) {
				if (server->queue->size > 0) {
					m = MessageQueue_pop(server->queue);
					evbuffer_add_printf(server->outbuf, "$%d\r\n", m->size);
					evbuffer_add(server->outbuf, m->message, m->size);
					evbuffer_add_printf(server->outbuf, "\r\n");
				}
				else {
					server_client_error(server, "NO_MESSAGES");
				}	
			}
			
			/* Invalid command */
			else {
				server_client_error(server, "INVALID_COMMAND");
			}
			free(request_line);
			evbuffer_drain(server->inbuf, len);
			printf("send phase 1\n");
		}
	}
	else {
		server->waiting_on_message = 0;
		/* This block of data should be a message body */
		message_body = (char *)malloc(sizeof(char)*(server->msg_length));
		check = bufferevent_read(bev, message_body, server->msg_length);
		if (len != (server->msg_length + 2)) {
			evbuffer_add_printf(server->outbuf, "%d:%d\n", len, server->msg_length);
			server_client_error(server, "MESSAGE_SIZE_INVALID");
			return;
		}
		if (check != server->msg_length) {
			server_client_error(server, "MESSAGE_NOT_FULLY_READ");
			return;
		}
		tmp = (char *)malloc(sizeof(char)*2);
		check = bufferevent_read(bev, tmp, 2);
		if (check != 2 || tmp[0] != '\r' || tmp[1] != '\n') {
			server_client_error(server, "EXPECTED_TERMINATOR");
			return;
		}
		m = Message_create(server->msg_priority, message_body, server->msg_length);
		if (m == NULL) {
			server_client_error(server, "MESSAGE_NOT_CREATED");
			return;
		}
		if (!MessageQueue_push(server->queue, m)) {
			server_client_error(server, "MESSAGE_NOT_PUSHED");
			return;
		}
		evbuffer_add_printf(server->outbuf, "+OK\r\n");
		evbuffer_drain(server->inbuf, len);
		free(message_body);
		free(tmp);
		printf("send phase 2\n");
		bufferevent_flush(bev, EV_WRITE, BEV_FINISHED);
	}
}

static void
server_event_cb(struct bufferevent *bev, short events, void *ctx)
{
	LineupServer *server = (LineupServer *)ctx;
	int finished = 0;
	if (events & BEV_EVENT_EOF) {
		finished = 1;
	}
	if (events & BEV_EVENT_ERROR) {
		perror("Error from bufferevent");
		finished = 1;
	}
	if (finished) {
		server->connections--;
		bufferevent_free(bev);
	}
}

static void
accept_conn_cb(struct evconnlistener *listener, evutil_socket_t fd,
               struct sockaddr *address, int socklen, void *ctx)
{
	LineupServer *server = (LineupServer *)ctx;
	struct event_base *base = evconnlistener_get_base(listener);
	struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	server->connections++;
	bufferevent_setcb(bev, server_read_cb_dummy, NULL, server_event_cb, ctx);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
}

/*
 * Starts a server on the specified port.
 * It loops pretty much forever.
 */
int
Server_start(int port)
{
	struct event_base *base;
	struct evconnlistener *listener;
	struct sockaddr_in sin;
	LineupServer server;
	
	base = event_base_new();
	if (!base) {
		fprintf(stderr, "couldn't open event base\n");
		return 0;
	}

	server.connections = 0;
	server.waiting_on_message = 0;
	server.msg_priority = 0;
	server.msg_length = 0;
	server.queue = MessageQueue_create();
	if (server.queue == NULL) {
		fprintf(stderr, "couldn't create message queue\n");
		return 0;
	}
	
	/* Clear the sockaddr before using it, in case there are extra
	 * platform-specific fields that can mess us up. */
	memset(&sin, 0, sizeof(sin));
	/* Listen on 0.0.0.0 */
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(0);
	sin.sin_port = htons(port);

	listener = evconnlistener_new_bind(base, accept_conn_cb, (void *)&server,
		LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
		(struct sockaddr*)&sin, sizeof(sin));
	if (!listener) {
		perror("couldn't create listener");
		return 0;
	}

	event_base_dispatch(base);
	
	return 1;
}

