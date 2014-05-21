/*
 * See LICENSE for licensing information
 */

#include "as.h"

#define PAGE_SIZE 4096
#define MAX_CLIENTS 1024

/* all state for as is stored here */
struct _AS {
	/* the function we use to log messages
	 * needs level, functionname, and format */
	ShadowLogFunc slogf;

	int ined;

	/* track if our client got a response and we can exit */
	int isDone;

	/* the two socket descriptor for the two communication directions */
	int ain;
	int aout;
	int asockin;
	int outport, inport;

	char buf[PAGE_SIZE];

	int good_data;

	int endread;
	
	int firstTime;

	in_addr_t hostIP, remoteIP;	

	GHashTable *hashmap;
};

/* if option is specified, run as client, else run as server */
static const char* USAGE = 
	"USAGE: autosys hostname_bind:LISTEN_PORT hostname_connect:OUT_PORT\n";


/* At the BEGINNING this is the proxy writer end, whose that 
   communicates with the real server (this would be a reader too then) */
static int _as_startWriter(AS* h) {
	h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"Writer: started.");
	return 0;
}

/* At the BEGINNING this is the proxy reader end, whose that 
   communicates with the real client (this would be a writer too then) */
static int _as_startReader(AS* h) {
	/* create the socket and get a socket descriptor */

	int true = 1;

	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_port = h->inport,
	};

	sin.sin_addr.s_addr = h->hostIP;

	//TODO: change the fd
	h->asockin = socket(AF_INET, SOCK_STREAM, 0);
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

	if (listen(h->asockin, 10)){
		h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start reader: error in listen");
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

static int _as_startAs(AS *h) {
	int ret;
	if ( (ret = _as_startWriter(h)) < 0 ) {
		/* Error case */
		return ret;
	}
	return _as_startReader(h);
}

AS* as_new(int argc, char* argv[], ShadowLogFunc slogf) {
	assert(slogf);

	struct addrinfo* hostInfo = NULL;
	in_addr_t inaddr = 0, outaddr = 0;
	char *hostname_bind;
	char *hostname_connect;
	char *inport;
	char *outport;
	char *tmp;


	if(argc != 3) {
		slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__, USAGE);
		return NULL;
	}

	asprintf(&tmp, "%s", argv[1]);
	hostname_bind = strsep(&tmp, ":");
	inport = tmp;

	asprintf(&tmp, "%s", argv[2]);
	hostname_connect = strsep(&tmp, ":");
	outport = tmp;

	printf("\t%s %s %s %s\n", hostname_bind, inport , hostname_connect, outport);

	char myhostname[1024];
	gethostname(myhostname, 1024);

	slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"myhostname %s", myhostname);

	/* use epoll to asynchronously watch events for all of our sockets */
	int inputEd = epoll_create(MAX_CLIENTS);
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

	char buf1[1024];
	/* DNS query */
	/* get the address in network order */
	if(strncasecmp(hostname_connect, "none", 4) == 0) {
		outaddr = htonl(INADDR_NONE);
	}else if(strncasecmp(hostname_connect, "localhost", 9) == 0) {
		outaddr = htonl(INADDR_LOOPBACK);
	}else if(strncasecmp(hostname_connect, "any", 9) == 0) {
		outaddr = htonl(INADDR_ANY);
	}else if (getaddrinfo(hostname_connect, NULL, NULL, &hostInfo) >= 0) {
		while (hostInfo->ai_next) hostInfo = hostInfo->ai_next;
	
		outaddr = ((struct sockaddr_in *)(hostInfo->ai_addr))->sin_addr.s_addr;
	} else{
		slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"Error in main getaddrinfo (%s)", strerror(errno));
		close(inputEd);
		return NULL;
	}
	perror("getaddrinfo: ");
	
	slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
		"%s inaddr (%s), %s outaddr (%s)", hostname_bind, inet_ntoa(*(struct in_addr *)&inaddr), hostname_connect, inet_ntop(AF_INET, &outaddr, buf1,100));



	/* get memory for the new state */
	AS* h = calloc(1, sizeof(AS));
	assert(h);

	h->outport = htons(atoi(outport));
	h->inport = htons(atoi(inport));
	h->ined = inputEd;
	h->slogf = slogf;
	h->isDone = 0;
	h->endread = 0;
	h->good_data = 0;
	h->hostIP = inaddr;
	h->remoteIP = outaddr;
	h->firstTime = 1;
	h->hashmap = g_hash_table_new(g_int_hash, g_int_equal);
	
	if (hostInfo)
		freeaddrinfo(hostInfo);

	/* extract the server hostname from argv if in client mode */
	int isFail = 0;
	isFail = _as_startAs(h);

	if(isFail) {
		as_free(h);
		return NULL;
	} else {
		return h;
	}

}

void as_free(AS* h) {
	assert(h);

	g_hash_table_destroy(h->hashmap);

	if(h->ined)
		close(h->ined);

	free(h);
}

static void _as_activateAs(AS* h, int sd, uint32_t events) {
	h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"In the activate %d.", sd);

	if(events & EPOLLOUT) {
		h->slogf(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__,
				"EPOLLOUT is set %d", sd);
	}
	if(events & EPOLLIN) {
		h->slogf(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__,
				"EPOLLIN is set %d", sd);
	}

#ifdef AS_DEBUG
	usleep(500000);
#endif

	int changed = 1;
	if (sd == h->asockin ) { /* Due to the shadow accept non-blocking issue */ 
		gint *cin, *cout,
			*scout, *scin;
		struct sockaddr_in son = {
			.sin_family = AF_INET,
			.sin_port = h->outport
		};

		son.sin_addr.s_addr = h->remoteIP;

		if(h->isDone == 1) {
			if ( _as_startWriter(h) < 0 ){
				exit(EXIT_FAILURE);
			}
			h->isDone = 0;
		}
		
		cin = g_new0(gint, 1);
		*cin = accept(h->asockin, NULL, NULL);
		h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"Reader: accepted connection %d (%s)", *cin, strerror(errno));

		cout = g_new0(gint, 1);
		*cout = socket(AF_INET, SOCK_STREAM, 0);
		connect(*cout, &son, sizeof(struct sockaddr_in));

		h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"Reader: connected %d (%s)", *cout, strerror(errno));

		printf("Reader Replaced %d:%d\n", *cin, *cout);
		g_hash_table_replace(h->hashmap, cin, cout);
		g_hash_table_replace(h->hashmap, cout, cin);

		struct epoll_event ev;
		ev.events = EPOLLIN;
		ev.data.fd = *cin;
		if (epoll_ctl(h->ined, EPOLL_CTL_ADD, *cin, &ev) == -1){
			h->slogf(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
					"unable to start reader: error in epoll_ctl_add (%s)", strerror(errno));
			return;
		}
	}
	else if((events & EPOLLOUT) && (h->good_data > 0)) {
		int lsd = sd;
		int *fd = g_hash_table_lookup(h->hashmap, &lsd);
		int osd = *fd;

		h->good_data = send(sd, h->buf, (size_t)h->good_data, 0);

		h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"debug point (size %d)", h->good_data);

		if(h->good_data) {
			h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
					"successfully sent  message (size %d)", h->good_data);
		} else {
			h->slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
					"unable to send message");
		}
		h->good_data = 0;

		/* removing EPOLLOUT from h->aout */
		struct epoll_event ev = {
			.events = EPOLLIN,
			.data.fd = sd
		};
		epoll_ctl(h->ined, EPOLL_CTL_MOD, sd, &ev);

		/* removing EPOLLOUT from h->aout */
		ev.events = EPOLLIN;
		ev.data.fd = osd;
		epoll_ctl(h->ined, EPOLL_CTL_MOD, osd, &ev);

		if (h->endread) //TODO: check if writend too
		{
			h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
					"stream ended");
		}
	} else if((events & EPOLLIN) && (h->good_data == 0)) {


		h->endread = 0;
		h->good_data = recv(sd, h->buf, (size_t)PAGE_SIZE, 0);
		h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			 "received %d %s\n", h->good_data, strerror(errno));
		/* log result */
		if(h->good_data > 0) {
			int i;
			h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
					"successfully received a message:");
#ifdef AS_DEBUG
			for (i = 0; i < h->good_data; i++)
				printf("%c", h->buf[i]);
			printf("\n");
#endif
		} else {
			h->slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
					"unable to receive message");
		}

		/* tell epoll we no longer want to watch this socket */
		if (h->good_data <= PAGE_SIZE &&  h->good_data > 0)
		{
			int lsd = sd;
			int *fd = g_hash_table_lookup(h->hashmap, &lsd);
			int osd = *fd;
			if(h->good_data < PAGE_SIZE)
				h->endread = 1;
			struct epoll_event ev = {
				.events = EPOLLOUT|EPOLLIN,
			};
			ev.data.fd = osd;

			h->slogf(SHADOW_LOG_LEVEL_MESSAGE,
					__FUNCTION__,  "received a packet: %s\n",
					/* XXX Wrong! can be > the accept than the connection
					 * but that the minor of problems that we have now :) */
					(sd<osd?  "client -> server" : "client <- server"));

			if(epoll_ctl(h->ined, EPOLL_CTL_ADD, osd, &ev))
				epoll_ctl(h->ined, EPOLL_CTL_MOD, osd, &ev);
		}
		else {
			epoll_ctl(h->ined, EPOLL_CTL_DEL, sd, NULL);
			close(sd);
			int lsd = sd;
			int *fd = g_hash_table_lookup(h->hashmap, &lsd);
			int osd = *fd;
			epoll_ctl(h->ined, EPOLL_CTL_DEL, osd, NULL);
			close(osd);
			
			h->slogf(SHADOW_LOG_LEVEL_MESSAGE,
					__FUNCTION__, "closing the connections");

			h->isDone = 1;
		}
	}
}

void as_ready(AS* h) {
	assert(h);

	/* collect the events that are ready */
	struct epoll_event epevs[MAX_CLIENTS];
	int nfds = epoll_wait(h->ined, epevs, MAX_CLIENTS, 0);
	
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
		_as_activateAs(h, d, e);
	}
}

/* return the file descriptor to read from */
int as_getEpollDescriptor(AS* h) {
	assert(h);
	return h->ined;
}

int as_isDone(AS* h) {
	assert(h);
	return h->isDone;
}

int as_resetAccept(AS* h){
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
	
	h->firstTime = 1;
	
	return 0;
}
