#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "server.h"
#include "message.h"

/* Reacts to read events on the input buffer. This is the dispatching
   function and will react to commands received on the server. */
static void
server_read_cb(struct bufferevent *bev, void *ctx)
{
	/* This callback is invoked when there is data to read on bev. */
	struct evbuffer *input = bufferevent_get_input(bev);
	struct evbuffer *output = bufferevent_get_output(bev);
	size_t len = evbuffer_get_length(input);
	char *tmp;
	char *request_line = evbuffer_readln(input, &len, EVBUFFER_EOL_CRLF);
	if (request_line) {
		if (!strncmp(request_line, "EXIT ", 5)) {
			free(ctx);
			bufferevent_free(bev);
		}
		else if (!strncmp(request_line, "PUSH ", 5)) {
			evbuffer_add_printf(output, "PUSH command received\n");
			tmp = (char *)malloc(sizeof(char)*(len-5));
			strncpy(tmp, request_line + 5, len - 5);
			evbuffer_add_printf(output, "Job: %s\n", tmp);
		}
		else if (!strncmp(request_line, "POP ", 4)) {
			evbuffer_add_printf(output, "POP command received\n");
		}
		else {
			/* Copy all the data from the input buffer to the output buffer. */
			evbuffer_add_printf(output, "%s\n", request_line);
		}
		free(request_line);
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
	/* We got a new connection! Set up a bufferevent for it. */
	struct event_base *base = evconnlistener_get_base(listener);
	struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev, server_read_cb, NULL, server_event_cb, NULL);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
}

int
Server_start(int port)
{
	struct event_base *base;
	struct evconnlistener *listener;
	struct sockaddr_in sin;
	
	base = event_base_new();
	if (!base) {
		fprintf(stderr, "couldn't open event base");
		return 0;
	}
	
	/* Clear the sockaddr before using it, in case there are extra
	 * platform-specific fields that can mess us up. */
	memset(&sin, 0, sizeof(sin));
	/* Listen on 0.0.0.0 */
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(0);
	sin.sin_port = htons(port);

	listener = evconnlistener_new_bind(base, accept_conn_cb, NULL,
		LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
		(struct sockaddr*)&sin, sizeof(sin));
	if (!listener) {
		perror("Couldn't create listener");
		return 0;
	}

	event_base_dispatch(base);
	
	return 1;
}

