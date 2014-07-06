/*
 * See LICENSE for licensing information
 */


#define _GNU_SOURCE 
#include <stdio.h>

#include "simpletcp.h"
#define SERVER 0
#define CLIENT 1

/* all state for simpletcp is stored here */
struct _SimpleTCP {
	/* the function we use to log messages
	 * needs level, functionname, and format */
	ShadowLogFunc slogf;

	/* the epoll descriptor to which we will add our sockets.
	 * we use this descriptor with epoll to watch events on our sockets. */
	int ed;

	/* track if our client got a response and we can exit */
	int isDone;

	struct {
		int sd;
		int serverPort;
		in_addr_t serverIP;
	} client;

	struct {
		int sd;
		int bindPort;
	} server;
};

/* if option is specified, run as client, else run as server */
static const char* USAGE = "USAGE: simpletcp c|s hostname:PORT\n";

static int _simpletcp_startClient(SimpleTCP* h) {
	int res;

	/* create the client socket and get a socket descriptor */
	h->client.sd = socket(AF_INET, (SOCK_STREAM | SOCK_NONBLOCK), 0);
	if(h->client.sd == -1) {
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start client: error in socket");
		return -1;
	}

	/* our client socket address information for connecting to the server */
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = h->client.serverIP;
	addr.sin_port = htons(h->client.serverPort);

	/* connect to server. since we are non-blocking, we expect this to 
	   return EINPROGRESS */
	res = connect(h->client.sd, (struct sockaddr *) &addr, sizeof(addr));
	if (res == -1 && errno != EINPROGRESS) {
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start client: error in connect");
		return -1;
	}

	/* specify the events to watch for on this socket.
	 * the client wants to know when it can send a simpletcp message. */
	struct epoll_event ev;
	ev.events = EPOLLOUT;
	ev.data.fd = h->client.sd;

	/* start watching the client socket */
	res = epoll_ctl(h->ed, EPOLL_CTL_ADD, h->client.sd, &ev);
	if(res == -1) {
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start client: error in epoll_ctl");
		return -1;
	}

	/* success! */
	return 0;
}

static int _simpletcp_startServer(SimpleTCP* h) {
	/* create the socket and get a socket descriptor */
	h->server.sd = socket(AF_INET, (SOCK_STREAM | SOCK_NONBLOCK), 0);
	if (h->server.sd == -1) {
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start server: error in socket");
		return -1;
	}

	/* setup the socket address info, client has outgoing connection to server */
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(h->server.bindPort);

	/* bind the socket to the server port */
	int res = bind(h->server.sd, (struct sockaddr *)&addr, sizeof(addr));
	if (res == -1) {
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start server: error in bind");
		return -1;
	}

	/* set as server socket that will listen for clients */
	res = listen(h->server.sd, 100);
	if (res == -1) {
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start server: error in listen");
		return -1;
	}

	/* specify the events to watch for on this socket.
	 * the server wants to know when a client is connecting. */
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = h->server.sd;

	/* start watching the server socket */
	res = epoll_ctl(h->ed, EPOLL_CTL_ADD, h->server.sd, &ev);
	if(res == -1) {
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start server: error in epoll_ctl");
		return -1;
	}

	/* success! */
	return 0;
}

in_addr_t dnsQuery(char *hostname)
{
	struct addrinfo* hostInfo = NULL;
	in_addr_t addr;
	if(strncasecmp(hostname, "none", 4) == 0) {
		addr = htonl(INADDR_NONE);
	}else if(strncasecmp(hostname, "localhost", 9) == 0) {
		addr = htonl(INADDR_LOOPBACK);
	}else if(strncasecmp(hostname, "any", 9) == 0) {
		addr = htonl(INADDR_ANY);
	}else if (getaddrinfo(hostname, NULL, NULL, &hostInfo) >= 0) {
		while (hostInfo->ai_next) hostInfo = hostInfo->ai_next;

		addr = ((struct sockaddr_in *)(hostInfo->ai_addr))->sin_addr.s_addr;

	} else{
		return -1;
	}
	if (hostInfo)
		freeaddrinfo(hostInfo);
	return addr;
}

SimpleTCP* simpletcp_new(int argc, char* argv[], ShadowLogFunc slogf) {
	assert(slogf);

	int side = SERVER;
	char *hostname, *port, myhostname[1024];
	char *tmp;

	if (argc < 3 && argv[1][0] != 's' && argv[1][0] != 'c') {
		slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__, USAGE);
		return NULL;
	}

	if (argv[1][0] == 'c') //TODO: I know, this is very ugly 
		side  = CLIENT;

	asprintf(&tmp, "%s", argv[2]);
	hostname = strsep(&tmp, ":");
	port = tmp;
	
	printf("\t%s %s\n", hostname, port);
	
	gethostname(myhostname, 1024);
	slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__, 
		"myhostname %s", myhostname);

	/* use epoll to asynchronously watch events for all of our sockets */
	int mainEpollDescriptor = epoll_create(1);
	if(mainEpollDescriptor == -1) {
		slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
			"Error in main epoll_create");
		close(mainEpollDescriptor);
		return NULL;
	}

	/* get memory for the new state */
	SimpleTCP* h = calloc(1, sizeof(SimpleTCP));
	assert(h);


	h->ed = mainEpollDescriptor;
	h->slogf = slogf;
	h->isDone = 0;

	/* extract the server hostname from argv if in client mode */
	int isFail = 0;
	if (side == CLIENT) {
		/* client mode */
		h->client.serverIP = dnsQuery(hostname);
		if (h->client.serverIP == -1) {		
			slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"Error in dnsQuery");
			return NULL;
		}
		h->client.serverPort = htons(atoi(port));
		isFail = _simpletcp_startClient(h);
	} else {
		/* server mode */
		h->server.bindPort = htons(atoi(port));
		isFail = _simpletcp_startServer(h);
	}

	if(isFail) {
		simpletcp_free(h);
		return NULL;
	} else {
		return h;
	}
}

void simpletcp_free(SimpleTCP* h) {
	assert(h);

	if(h->client.sd)
		close(h->client.sd);
	if(h->ed)
		close(h->ed);

	free(h);
}

static void _simpletcp_activateClient(SimpleTCP* h, int sd, uint32_t events) {
	ssize_t numBytes = 0;
	char message[10];
	assert(h->client.sd == sd);

	if(events & EPOLLOUT) {
		h->slogf(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__, "EPOLLOUT is set");
	}
	if(events & EPOLLIN) {
		h->slogf(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__, "EPOLLIN is set");
	}

	/* to keep things simple, there is explicitly no resilience here.
	 * we allow only one chance to send the message and one to receive the response.
	 */
	if(events & EPOLLOUT) {
		/* the kernel can accept data from us,
		 * and we care because we registered EPOLLOUT on sd with epoll */

		/* prepare the message */
		memset(message, 0, (size_t)10);
		snprintf(message, 10, "%s", "Hello?");

		/* send the message */
		numBytes = send(sd, message, (size_t)6, 0);

		/* log result */
		if(numBytes == 6) {
			h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
					"successfully sent '%s' message", message);
		} else {
			h->slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
					"unable to send message");
		}

		/* tell epoll we don't care about writing anymore */
		struct epoll_event ev;
		memset(&ev, 0, sizeof(struct epoll_event));
		ev.events = EPOLLIN;
		ev.data.fd = sd;
		epoll_ctl(h->ed, EPOLL_CTL_MOD, sd, &ev);
	} else if(events & EPOLLIN) {
		/* there is data available to read from the kernel,
		 * and we care because we registered EPOLLIN on sd with epoll */

		/* prepare to accept the message */
		memset(message, 0, (size_t)10);

		numBytes = recv(sd, message, (size_t)6, 0);

		/* log result */
		if(numBytes > 0) {
			h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
					"successfully received '%s' message", message);
		} else {
			h->slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
					"unable to receive message");
		}

		/* tell epoll we no longer want to watch this socket */
		epoll_ctl(h->ed, EPOLL_CTL_DEL, sd, NULL);

		close(sd);
		h->client.sd = 0;
		h->isDone = 1;
	}
}

static void _simpletcp_activateServer(SimpleTCP* h, int sd, uint32_t events) {
	ssize_t numBytes = 0;
	char message[10];
	struct epoll_event ev;

	if(events & EPOLLOUT) {
		h->slogf(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__, "EPOLLOUT is set");
	}
	if(events & EPOLLIN) {
		h->slogf(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__, "EPOLLIN is set");
	}

	if(sd == h->server.sd) {
		/* data on a listening socket means a new client connection */
		assert(events & EPOLLIN);

		/* accept new connection from a remote client */
		int newClientSD = accept(sd, NULL, NULL);
		h->slogf(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__, 
			"ACCEPT: %s", strerror(errno));

		/* now register this new socket so we know when its ready */
		memset(&ev, 0, sizeof(struct epoll_event));
		ev.events = EPOLLIN;
		ev.data.fd = newClientSD;
		epoll_ctl(h->ed, EPOLL_CTL_ADD, newClientSD, &ev);
	} else {
		/* a client is communicating with us over an existing connection */
		if(events & EPOLLIN) {
			/* prepare to accept the message */
			memset(message, 0, (size_t)10);

			numBytes = recv(sd, message, (size_t)6, 0);

			/* log result */
			if(numBytes > 0) {
				h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__, 
					"successfully received '%s' message", 
					message);
			} else if(numBytes == 0){
				/* client got response and closed */
				/* tell epoll we no longer want to watch this socket */
				epoll_ctl(h->ed, EPOLL_CTL_DEL, sd, NULL);
				close(sd);
			} else {
				h->slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
					"unable to receive message");
			}

			/* tell epoll we want to write the response now */
			memset(&ev, 0, sizeof(struct epoll_event));
			ev.events = EPOLLOUT;
			ev.data.fd = sd;
			epoll_ctl(h->ed, EPOLL_CTL_MOD, sd, &ev);
		} else if(events & EPOLLOUT) {
			/* prepare the response message */
			memset(message, 0, (size_t)10);
			snprintf(message, 10, "%s", "World!");

			/* send the message */
			numBytes = send(sd, message, (size_t)6, 0);

			/* log result */
			if(numBytes == 6) {
				h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"successfully sent '%s' message", message);
			} else {
				h->slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
						"unable to send message");
			}

			/* now wait until we read 0 for client close event */
			memset(&ev, 0, sizeof(struct epoll_event));
			ev.events = EPOLLIN;
			ev.data.fd = sd;
			epoll_ctl(h->ed, EPOLL_CTL_MOD, sd, &ev);
		}
	}
}

void simpletcp_ready(SimpleTCP* h) {
	assert(h);

	/* collect the events that are ready */
	struct epoll_event epevs[10];
	int nfds = epoll_wait(h->ed, epevs, 10, 0);
	if(nfds == -1) {
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"error in epoll_wait");
		return;
	}

	/* activate correct component for every socket thats ready */
	for(int i = 0; i < nfds; i++) {
		int d = epevs[i].data.fd;
		uint32_t e = epevs[i].events;
		if(d == h->client.sd) {
			_simpletcp_activateClient(h, d, e);
		} else {
			_simpletcp_activateServer(h, d, e);
		}
	}
}

int simpletcp_getEpollDescriptor(SimpleTCP* h) {
	assert(h);
	return h->ed;
}

int simpletcp_isDone(SimpleTCP* h) {
	assert(h);
	return h->isDone;
}
