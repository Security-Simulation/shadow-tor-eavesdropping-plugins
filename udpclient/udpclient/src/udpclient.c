/*
 * See LICENSE for licensing information
 */

#include "udpclient.h"
#include <linux/ip.h>
#include <arpa/inet.h>

#include <errno.h>

#define PAGE_SIZE 4096

/* all state for udpclient is stored here */
struct _UDPClient {
	/* the function we use to log messages
	 * needs level, functionname, and format */
	ShadowLogFunc slogf;

	int ined;

	/* track if our client got a response and we can exit */
	int isDone;

	/* the two socket descriptor for the two communication directions */
	int ain;
	int asockin;
	int inport;

	char buf[PAGE_SIZE];

	int good_data;

	int endread;
	
	in_addr_t hostIP;	
};

/* if option is specified, run udpclient client, else run udpclient server */
static const char* USAGE = 
	"USAGE: tor-udpclient hostname_bind:LISTEN_PORT\n";

/* At the BEGINNING this is the proxy reader end, whose that 
   communicates with the real client (this would be a writer too then) */
static int _udpclient_startReader(UDPClient* h) {
	/* create the socket and get a socket descriptor */
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_port = h->inport,
	};

	sin.sin_addr.s_addr = h->hostIP;

	//TODO: change the fd
	h->asockin = socket(AF_INET, SOCK_DGRAM, 0);
	if (h->asockin == -1){
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start reader: error in socket");
		return -1;
	}
	h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"Reader: create socket %d", h->asockin);

	if (bind(h->asockin, (struct sockaddr *)&sin, sizeof(struct sockaddr_in))){
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start reader: error in bind (%s)", strerror(errno));
		return -1;
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = h->asockin;
	if (epoll_ctl(h->ined, EPOLL_CTL_ADD, h->asockin, &ev) == -1){
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start reader: error in epoll_ctl (%s)", strerror(errno));
		return -1;
	}

	h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"Reader started.");

	return 0;
}

static int _udpclient_startUDPClient(UDPClient *h) {
	return _udpclient_startReader(h);
}

UDPClient* udpclient_new(int argc, char* argv[], ShadowLogFunc slogf) {
	assert(slogf);

	struct addrinfo* hostInfo = NULL;
	in_addr_t inaddr = 0;
	char *hostname_bind;
	char *inport;
	char *tmp;


	if(argc != 2) {
		slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__, USAGE);
		return NULL;
	}

	asprintf(&tmp, "%s", argv[1]);
	hostname_bind = strsep(&tmp, ":");
	inport = tmp;

	/* use epoll to asynchronously watch events for all of our sockets */
	int inputEd = epoll_create(1);
	if(inputEd == -1) {
		slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"Error in main epoll_create");
		close(inputEd);
		return NULL;
	}
	/* DNS query */
	/* get the address in network order */
	if(strncasecmp(hostname_bind, "none", 4) == 0) {
		inaddr = htonl(INADDR_NONE);
	}else if(strncasecmp(hostname_bind, "localhost", 9) == 0) {
		inaddr = htonl(INADDR_LOOPBACK);
	}else if(strncasecmp(hostname_bind, "any", 9) == 0) {
		inaddr = htonl(INADDR_ANY);
	}else if (getaddrinfo(hostname_bind, NULL, NULL, &hostInfo) >= 0) {		
		while (hostInfo->ai_next) hostInfo = hostInfo->ai_next;

		inaddr = ((struct sockaddr_in *)(hostInfo->ai_addr))->sin_addr.s_addr;
	
	} else{
		slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"Error in main getaddrinfo (%s)", strerror(errno));
		close(inputEd);
		return NULL;
	}
	memset(&hostInfo, 0, sizeof(hostInfo));	

	slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
		"%s inaddr (%s)", hostname_bind, inet_ntoa(*(struct in_addr *)&inaddr));

	/* get memory for the new state */
	UDPClient* h = calloc(1, sizeof(UDPClient));
	assert(h);

	h->inport = htons(atoi(inport));
	h->ined = inputEd;
	h->slogf = slogf;
	h->isDone = 0;
	h->endread = 0;
	h->good_data = 0;
	h->hostIP = inaddr;
	
	if (hostInfo)
		freeaddrinfo(hostInfo);

	/* extract the server hostname from argv if in client mode */
	int isFail = 0;
	isFail = _udpclient_startUDPClient(h);

	if(isFail) {
		udpclient_free(h);
		return NULL;
	} else {
		return h;
	}

}

void udpclient_free(UDPClient* h) {
	assert(h);

	if(h->ined)
		close(h->ined);

	free(h);
}


static void _udpclient_activateAs(UDPClient* h, int sd, uint32_t events) {
	h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"In the activate %x.", events);

#ifdef UDPClient_DEBUG
	usleep(500000);
#endif
	
	if(events & EPOLLIN) {
		char buf[4096];
		h->slogf(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__,
				"EPOLLIN is set %d", sd);
		h->endread = 0;
		h->good_data = recv(sd, buf, (size_t)PAGE_SIZE, 0);
		h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			 "received %d %s\n", h->good_data, strerror(errno));
		/* log result */
		if(h->good_data > 0) {
			h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
					"successfully received a message:");
#ifdef UDPClient_DEBUG
			int i;
			for (i = 0; i < h->good_data; i++)
				printf("%c", h->buf[i]);
			printf("\n");
#endif
			/* XXX process the message */
		}
	}
}

void udpclient_ready(UDPClient* h) {
	assert(h);

	/* collect the events that are ready */
	struct epoll_event epevs[10];
	int nfds = epoll_wait(h->ined, epevs, 10, 0);
	
	h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"As_ready : Successfull");

	if(nfds == -1) {
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"error in epoll_wait");
		return;
	}

	/* activate correct component for every socket thats ready */
	for(int i = 0; i < nfds; i++) {
		int d = epevs[i].data.fd;
		uint32_t e = epevs[i].events;
		/* if (e) */
			_udpclient_activateAs(h, d, e);
	}
}

/* return the file descriptor to read from */
int udpclient_getEpollDescriptor(UDPClient* h) {
	assert(h);
	return h->ined;
}

int udpclient_isDone(UDPClient* h) {
	assert(h);
	return h->isDone;
}

int udpclient_resetAccept(UDPClient* h){
	h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"resetting the whole thing");
	
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = h->asockin;
/*	if (epoll_ctl(h->ined, EPOLL_CTL_DEL, h->aout, NULL) == -1){
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to reset everything: error in epoll_ctl_del (%s)", strerror(errno));
		return -1;
	}*/
	if (epoll_ctl(h->ined, EPOLL_CTL_ADD, h->asockin, &ev) == -1){
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to reset everything: error in epoll_ctl_add (%s)", strerror(errno));
		return -1;
	}
	
	return 0;
}
