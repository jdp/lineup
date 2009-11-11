struct lineup_job
{
	int priority;
	char* job;
};

#define PQITEM struct lineup_job *
#define PRIORITY(p) (p->priority)

struct pqueue
{
	int size, avail, step;
	PQITEM *items;
};

/* prototypes from server.c */

static void
server_read_cb(struct bufferevent *, void *);

static void
server_event_cb(struct bufferevent *, short events, void *);

static void
accept_conn_cb(struct evconnlistener *, evutil_socket_t, struct sockaddr *,
               int, void *);
