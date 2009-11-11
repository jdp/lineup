#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

static void server_read_cb(struct bufferevent *, void *);
static void server_event_cb(struct bufferevent *, short events, void *);
static void accept_conn_cb(struct evconnlistener *, evutil_socket_t, struct sockaddr *,
                           int, void *);
int         Server_start(int);

#endif
