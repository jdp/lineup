#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "server.h"
#include "message.h"

typedef struct lineup_server_t
{
	int connections;
	MessageQueue *queue;
	int waiting_on_message, msg_priority, msg_length;
} LineupServer;

/* Reacts to read events on the input buffer. This is the dispatching
   function and will react to commands received on the server. */
static void
server_read_cb(struct bufferevent *bev, void *ctx)
{
	struct evbuffer *input, *output;
	size_t len;
	LineupServer *server;
	Message *m;
	char *tmp, *request_line, *parser, *message_body;
	int priority, message_length, check;
	
	server = (LineupServer *)ctx;
	input = bufferevent_get_input(bev);
	output = bufferevent_get_output(bev);
	len = evbuffer_get_length(input);
	
	/* Since we're not waiting on a message, look for commands */
	if (!server->waiting_on_message) {
	
		/* Look for commands */
		request_line = evbuffer_readln(input, &len, EVBUFFER_EOL_CRLF);
		parser = request_line;
		
		if (request_line) {
	
			/* Respond to the EXIT command */
			if (!strncmp(request_line, "EXIT", 4)) {
				bufferevent_free(bev);
			}
		
			/* Respond to the PUSH command */
			else if (!strncmp(request_line, "PUSH ", 5)) {
				/* Read priority */
				parser += 5;
				priority = 0;
				if (isdigit(*parser)) {
					while (isdigit(*parser) && (parser <= (request_line + len))) {
						priority = priority * 10 + (*parser - '0');
						parser++;
					}
				}
				else {
					server->waiting_on_message = 0;
					evbuffer_add_printf(output, "-EXPECTED_PRIORITY\r\n");
					return;
				}
				/* Read message length */
				if (!isspace(*parser)) {
					server->waiting_on_message = 0;
					evbuffer_add_printf(output, "-EXPECTED_MESSAGE_LENGTH\r\n");
					return;
				}
				parser++;
				message_length = 0;
				if (isdigit(*parser)) {
					while (isdigit(*parser) && (parser <= (request_line + len))) {
						message_length = message_length * 10 + (*parser - '0');
						parser++;
					}
				}
				else {
					server->waiting_on_message = 0;
					evbuffer_add_printf(output, "-UNEXPECTED_INPUT\r\n");
					return;
				}
				/* Next block of data should be a message body */
				server->msg_length = message_length;
				server->msg_priority = priority;
				server->waiting_on_message = 1;
			}
			
			/* Respond to POP command */
			else if (!strncmp(request_line, "POP", 3)) {
				if (server->queue->size > 0) {
					m = MessageQueue_pop(server->queue);
					evbuffer_add_printf(output, "$%d\r\n", m->size);
					evbuffer_add(output, m->message, m->size);
					evbuffer_add_printf(output, "\r\n");
				}
				else {
					evbuffer_add_printf(output, "-NO_MESSAGES\n");
				}	
			}
			
			/* Invalid command */
			else {
				evbuffer_add_printf(output, "-INVALID_COMMAND\r\n");
			}
			free(request_line);
		}
	}
	else {
		server->waiting_on_message = 0;
		/* This block of data should be a message body */
		message_body = (char *)malloc(sizeof(char)*(server->msg_length));
		check = bufferevent_read(bev, message_body, server->msg_length);
		if (len != (server->msg_length + 2)) {
			evbuffer_add_printf(output, "-MESSAGE_SIZE_INVALID %d:%d\n", len-2, server->msg_length);
			return;
		}
		if (check != server->msg_length) {
			evbuffer_add_printf(output, "-MESSAGE_NOT_FULLY_READ %d:%d\r\n",check,server->msg_length);
			return;
		}
		tmp = (char *)malloc(sizeof(char)*2);
		check = bufferevent_read(bev, tmp, 2);
		if (check != 2 || tmp[0] != '\r' || tmp[1] != '\n') {
			evbuffer_add_printf(output, "-EXPECTED_TERMINATOR\r\n");
			return;
		}
		m = Message_create(server->msg_priority, message_body, server->msg_length);
		if (m == NULL) {
			evbuffer_add_printf(output, "-MESSAGE_NOT_CREATED\r\n");
			return;
		}
		if (!MessageQueue_push(server->queue, m)) {
			evbuffer_add_printf(output, "-MESSAGE_NOT_PUSHED\r\n");
			return;
		}
		evbuffer_add_printf(output, "+OK\r\n");
		evbuffer_drain(input, len);
		free(message_body);
		free(tmp);
	}
}

static void
server_event_cb(struct bufferevent *bev, short events, void *ctx)
{
	if (events & BEV_EVENT_ERROR) {
		perror("Error from bufferevent");
	}
	if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
		bufferevent_free(bev);
	}
}

static void
accept_conn_cb(struct evconnlistener *listener, evutil_socket_t fd,
               struct sockaddr *address, int socklen, void *ctx)
{
	struct event_base *base = evconnlistener_get_base(listener);
	struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev, server_read_cb, NULL, server_event_cb, ctx);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
}

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

