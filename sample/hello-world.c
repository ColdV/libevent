/*
  This example program provides a trivial server program that listens for TCP
  connections on port 9995.  When they arrive, it writes a short message to
  each client connection, and closes each connection once it is flushed.

  Where possible, it exits cleanly in response to a SIGINT (ctrl-c).
*/


#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#ifndef _WIN32
#include <netinet/in.h>
#ifdef _XOPEN_SOURCE_EXTENDED
#include <arpa/inet.h>
#endif
#include <sys/socket.h>
#endif

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

static const char MESSAGE[] = "Hello, World!\n";

static const int PORT = 3308;

static void listener_cb(struct evconnlistener *, evutil_socket_t,
	struct sockaddr *, int socklen, void *);
static void conn_writecb(struct bufferevent *, void *);
static void conn_readcb(struct bufferevent *, void *);
static void conn_eventcb(struct bufferevent *, short, void *);
static void signal_cb(evutil_socket_t, short, void *);

int
main(int argc, char **argv)
{
	struct event_base *base;
	struct evconnlistener *listener;
	struct event *signal_event;

	struct sockaddr_in sin;
#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);

	// evthread_use_windows_threads();
	// struct event_config *cfg = event_config_new();
	// event_config_set_flag(cfg, EVENT_BASE_FLAG_STARTUP_IOCP);
	// base = event_base_new_with_config(cfg);
	base = event_base_new();
#else
	base = event_base_new();
#endif

	if (!base) {
		fprintf(stderr, "Could not initialize libevent!\n");
		return 1;
	}


	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);

	listener = evconnlistener_new_bind(base, listener_cb, (void *)base,
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1, (struct sockaddr *)&sin,
		sizeof(sin));

	if (!listener) {
		fprintf(stderr, "Could not create a listener!\n");
		return 1;
	}

	//signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);

	/*
    if (!signal_event || event_add(signal_event, NULL) < 0) {
		fprintf(stderr, "Could not create/add a signal event!\n");
		return 1;
	}
    */

	event_base_dispatch(base);

	evconnlistener_free(listener);
	event_free(signal_event);
	event_base_free(base);

	printf("done\n");
	return 0;
}

int timenow = 0;
static void
listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
	struct sockaddr *sa, int socklen, void *user_data)
{
	static int cnt = 0;
	if (0 == cnt)
		timenow = time(NULL);

	++cnt;
	if (10000 == cnt) {
		printf("accept cost time:%d\n", time(NULL) - timenow);
	}

	struct event_base *base = user_data;
	struct bufferevent *bev;

	bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev) {
		fprintf(stderr, "Error constructing bufferevent!");
		event_base_loopbreak(base);
		return;
	}
	bufferevent_setcb(bev, conn_readcb, conn_writecb, conn_eventcb, NULL);
	bufferevent_enable(bev, EV_WRITE);
	// bufferevent_disable(bev, EV_READ);
	bufferevent_enable(bev, EV_READ);

	// bufferevent_write(bev, MESSAGE, strlen(MESSAGE));
}

static void
conn_writecb(struct bufferevent *bev, void *user_data)
{
	struct evbuffer *output = bufferevent_get_output(bev);
	if (evbuffer_get_length(output) == 0) {
		printf("flushed answer\n");
		// bufferevent_free(bev);
	}
}

static void
conn_readcb(struct bufferevent *bev, void *user_data)
{
	struct evbuffer *input = bufferevent_get_input(bev);
	printf("into conn_readcb\n");
	int readlen = evbuffer_get_length(input);

	char *data = (char *)malloc(readlen);

	memset(data, 0, readlen);

	if (0 < readlen) {
		bufferevent_read(bev, data, readlen);
		//printf("recv data:%s\n", data);
		int sendlen = bufferevent_write(bev, data, readlen);
		//bufferevent_disable(bev, EV_WRITE);
	}
}


static void
conn_eventcb(struct bufferevent *bev, short events, void *user_data)
{
	if (events & BEV_EVENT_EOF) {
		printf("Connection closed.\n");
		bufferevent_free(bev);
	} else if (events & BEV_EVENT_ERROR) {
		printf("Got an error on the connection: %s\n",
			strerror(errno)); /*XXX win32*/
		bufferevent_free(bev);
	}
	/* None of the other events can happen here, since we haven't enabled
	 * timeouts */
	// bufferevent_free(bev);
}

static void
signal_cb(evutil_socket_t sig, short events, void *user_data)
{
	struct event_base *base = user_data;
	struct timeval delay = {2, 0};

	printf("Caught an interrupt signal; exiting cleanly in two seconds.\n");

	event_base_loopexit(base, &delay);
}
