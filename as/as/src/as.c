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

struct partner_s {
	int sd;
	ssize_t bufsize;
	char *buf;
};

/* if option is specified, run as client, else run as server */
static const char* USAGE = 
"USAGE: autosys hostname_bind:LISTEN_PORT hostname_connect:OUT_PORT\n";

/* TODO forse aggiungere un controllo per il mod? */
int setEPOLL(int ed, int sd, uint32_t events)
{
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = sd;
	if(epoll_ctl(ed, EPOLL_CTL_ADD, sd, &ev))
		epoll_ctl(ed, EPOLL_CTL_MOD, sd, &ev);
	return 0;
}

int resetEPOLL(int ed, int sd)
{
	epoll_ctl(ed, EPOLL_CTL_DEL, sd, NULL);
	return 0;
}
#define setEPOLLIN(ed, sd) setEPOLL(ed, sd, EPOLLIN)
#define setEPOLLOUT(ed, sd) setEPOLL(ed, sd, EPOLLOUT)
#define setEPOLLALL(ed, sd) setEPOLL(ed, sd, EPOLLOUT|EPOLLIN)

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

/* Function to trace udp in tor */
void udpTrace()
{
	return;
}

void closeConnections(AS *h, int sd, struct partner_s *partner)
{
	int psd = partner->sd;
	int lsd = sd;

	h->slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
			"Closing connections");

	resetEPOLL(h->ined, sd);
	close(sd);

	resetEPOLL(h->ined, psd);
	close(psd);

	/* Disaster recovery */
	if (partner->bufsize != 0) {
		h->slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
			 "A free was left out, something nasty is coming! :(");
		free(partner->buf);
	}

	g_hash_table_remove(h->hashmap, &lsd);
	g_hash_table_remove(h->hashmap, &psd);
	h->isDone = 1;
}
/*
 * Called when a new connection is established:
 * create the two connection end-points hashmap entries
 * and set EPOLLIN for them.
 */
void handleNewConnection(AS *h, int sd)
{
	/* Due to the shadow accept non-blocking issue */
	gint	*cin, *cout;
	struct partner_s	*inEnd, *outEnd;

	/* Build a new connection to the server */
	struct sockaddr_in son = {
		.sin_family = AF_INET,
		.sin_port = h->outport
	};
	son.sin_addr.s_addr = h->remoteIP;
	/* XXX CHECK */
	if(h->isDone == 1) {
		if (_as_startWriter(h) < 0)
			exit(EXIT_FAILURE);
		h->isDone = 0;
	}
	/* END CHECK */

	cin = g_new0(gint, 1);
	*cin = accept(h->asockin, NULL, NULL);

	h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"Reader: accepted connection %d (%s)", *cin,
			strerror(errno));

	udpTrace();

	cout = g_new0(gint, 1);
	*cout = socket(AF_INET, SOCK_STREAM, 0);
	if (connect(*cout, &son, sizeof(struct sockaddr_in))) {
		h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"Error in connect (%s)\n", strerror(errno));
		close(*cin);
		close(*cout);
		return;
	}

	h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"Reader: connected %d (%s)", *cout, strerror(errno));

	printf("Reader Replaced %d:%d\n", *cin, *cout);

	inEnd = g_new0(struct partner_s, 1);
	outEnd = g_new0(struct partner_s, 1);

	outEnd->sd = *cout;
	outEnd->bufsize = 0;
	g_hash_table_replace(h->hashmap, cin, outEnd);

	inEnd->sd = *cin;
	inEnd->bufsize = 0;
	g_hash_table_replace(h->hashmap, cout, inEnd);

	if(setEPOLLIN(h->ined, *cin))
		h->slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
				"Reader, error in epollin (1): %s",
				strerror(errno));

	if(setEPOLLIN(h->ined, *cout))
		h->slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
				"Reader, error in epollin (2): %s",
				strerror(errno));
}

#define IS_NEW_CONNECTION(sd) (sd == h->asockin)

/*
 * Called when sd can read and set the buffer:
 * reset the input file descriptor and wait for a successfull write.
 */
int handleRead(AS *h, int sd, struct partner_s *partner)
{
	partner->buf = (char *)calloc(PAGE_SIZE, sizeof(char));
	if (partner->buf == NULL) {
		h->slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
			 "Error in calloc\n");
		return -1;
	}

	partner->bufsize = recv(sd, partner->buf, (size_t)PAGE_SIZE, 0);

	if (partner->bufsize <= 0)  {
		return 1;
	}

	partner->buf = (char *)realloc(partner->buf, partner->bufsize);

	if (partner->buf == NULL) {
		h->slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
			 "Error in realloc %d\n", partner->bufsize);
		return -1;
	}

	h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
		 "Received %d %s\n", partner->bufsize, strerror(errno));

	/* tell epoll we no longer want to watch this socket */
	if (partner->bufsize > 0) {
		h->slogf(SHADOW_LOG_LEVEL_MESSAGE,
			__FUNCTION__,  "Received a packet: %s\n",
				/* XXX Wrong! can be > the accept than the connection
				 * but that the minor of problems that we have now :) */
			(sd < partner->sd ? "client -> server" : "client <- server"));

		setEPOLLALL(h->ined, partner->sd);
		resetEPOLL(h->ined, sd);
	} else {
		/* Close the connection signal. */
		return 1;
	}
	return 0;
}

/*
 * Called when sd can write the buffer:
 * write the buffer and wait for something to read.
 */
int handleWrite(AS *h, int sd, int outsd, struct partner_s *me)
{
	ssize_t sentbytes = -1;
	sentbytes = send(sd, me->buf, (size_t)me->bufsize, 0);
	perror("send");

	if(sentbytes) {
		h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			 "successfully sent  message (size %d)", me->bufsize);
	} else {
		h->slogf(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
			 "unable to send message");
	}
	me->bufsize = 0;

	/* security reason, nobody can read the buffer... */
	memset(me->buf, 0, me->bufsize);
	free(me->buf);

	/* removing EPOLLOUT */
	setEPOLLIN(h->ined, sd);
	setEPOLLIN(h->ined, outsd);

	return 0;
}

static void _as_activateAs(AS* h, int sd, uint32_t events) {
	struct partner_s *partner;
	int lsd = sd;
	partner = g_hash_table_lookup(h->hashmap, &lsd);

	if (partner == NULL && !IS_NEW_CONNECTION(sd)) {
		h->slogf(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__,
				"Null pointer in lookup of %d.", sd);
		_exit(EXIT_FAILURE);
	}

	h->slogf(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"In the activate %d.", sd);

	if(events & EPOLLIN) {
		int res = 0;
		h->slogf(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__,
				"EPOLLIN is set %d", sd);
		if (IS_NEW_CONNECTION(sd))
			handleNewConnection(h, sd);
		else
			res = handleRead(h, sd, partner);

		if (res > 0) {
			/* close the connection, some error (like EPIPE)
			 * occurred in the read. */
			closeConnections(h, sd, partner);
		} else if (res < 0) {
			/* Fatal error occurred. */
			_exit(EXIT_FAILURE);
		}
	}

	if(events & EPOLLOUT) {
		struct partner_s *me = g_hash_table_lookup(h->hashmap, &partner->sd);
		h->slogf(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__,
				"EPOLLOUT is set %d", sd);
		handleWrite(h, sd, partner->sd, me);
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
			"unable to reset everything: error in epoll_ctl_add (%s)",
			strerror(errno));
		return -1;
	}

	h->firstTime = 1;

	return 0;
}
