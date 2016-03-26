/*
 * See LICENSE for licensing information
 */
#define _GNU_SOURCE
#include "tproxy.h"

#define PAGE_SIZE 4096
#define MAX_CLIENTS 1024

/* all state for as is stored here */
struct _TProxy {
	/* the function we use to log messages
	 * needs level, functionname, and format */
	TProxyLogFunc slogf;

	int ined;

	/* track if our client got a response and we can exit */
	int isDone;

	int asockin;
	int	outport,
		inport,
		traceport;

	in_addr_t	hostIP,
			remoteIP,
			traceIP;

	int serverMode;
	int tracerSocket;
	struct sockaddr_in tracerSin;

	GHashTable *hashmap;
};

struct partner_s {
	int sd;
	ssize_t bufsize;
	char *buf;
};

/* if option is specified, run as client, else run as server */
static const char* USAGE = 
	"USAGE: tproxy hostname_bind:LISTEN_PORT hostname_connect:OUT_PORT "
	"hostname_analyzer:ANALYZER_PORT [server]\n";

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

/* Prepare the proxy to trace the connections */
void prepareToTrace(int argc, char **argv, TProxy *h)
{
	if (argc > 4 && !strcmp("server", argv[4])) {
		printf("server mode activated\n");
		/* Server Mode */
		h->serverMode = 1;
	}

	h->tracerSocket = socket(AF_INET, SOCK_DGRAM, 0);
	h->tracerSin.sin_family = AF_INET;
	h->tracerSin.sin_port = h->traceport;
	h->tracerSin.sin_addr.s_addr = h->traceIP;
	return;
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

/* At the BEGINNING this is the proxy writer end, whose 
   communicates with the real server (this would be a reader too then) */
static int _tproxy_startWriter(TProxy* h) {
	h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "Writer: started.");
	return 0;
}

/* At the BEGINNING this is the proxy reader end, whose  
   communicates with the real client (this would be a writer too then) */
static int _tproxy_startReader(TProxy* h) {
	/* create the socket and get a socket descriptor */

	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_port = h->inport,
	};

	sin.sin_addr.s_addr = h->hostIP;

	h->asockin = socket(AF_INET, SOCK_STREAM, 0);
	if (h->asockin == -1){
		h->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
		         "unable to start reader: error in socket");
		return -1;
	}
	h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
	         "Reader: create socket %d", h->asockin);

	if (bind(h->asockin, (struct sockaddr *)&sin, sizeof(struct sockaddr_in))){
		h->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
		         "unable to start reader: error in bind (%s)", strerror(errno));
		return -1;
	}

	if (listen(h->asockin, 10)) {
		h->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
		         "unable to start reader: error in listen");
		return -1;
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = h->asockin;
	if (epoll_ctl(h->ined, EPOLL_CTL_ADD, h->asockin, &ev) == -1){
		h->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
		         "unable to start reader: error in epoll_ctl (%s)", strerror(errno));
		return -1;
	}

	h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "Reader started.");

	return 0;
}

static int _tproxy_startTProxy(TProxy *h) {
	int ret;
	if ( (ret = _tproxy_startWriter(h)) < 0 ) {
		/* Error case */
		return ret;
	}
	return _tproxy_startReader(h);
}

TProxy* tproxy_new(int argc, char* argv[], TProxyLogFunc slogf) {
	assert(slogf);

	in_addr_t	inaddr = 0,
			outaddr = 0,
			traceaddr = 0;
	char	*hostname_bind,
		*hostname_connect,
		*hostname_trace;

	char	*inport,
		*outport,
		*traceport;
	char *tmp;

	if(argc < 4) {
		slogf(G_LOG_LEVEL_WARNING, __FUNCTION__, USAGE);
		return NULL;
	}

	asprintf(&tmp, "%s", argv[1]);
	hostname_bind = strsep(&tmp, ":");
	inport = tmp;

	asprintf(&tmp, "%s", argv[2]);
	hostname_connect = strsep(&tmp, ":");
	outport = tmp;

	asprintf(&tmp, "%s", argv[3]);
	hostname_trace = strsep(&tmp, ":");
	traceport = tmp;

	printf("\t%s %s %s %s\n", hostname_bind, inport , hostname_connect, outport);

	char myhostname[1024];
	gethostname(myhostname, 1024);

	slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"myhostname %s", myhostname);

	/* use epoll to asynchronously watch events for all of our sockets */
	int inputEd = epoll_create(MAX_CLIENTS);
	if(inputEd == -1) {
		slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"Error in main epoll_create");
		close(inputEd);
		return NULL;
	}
	/* DNS query */
	/* get the address in network order */
	inaddr = dnsQuery(hostname_bind);
	outaddr = dnsQuery(hostname_connect);
	traceaddr = dnsQuery(hostname_trace);

	/* get memory for the new state */
	TProxy* h = calloc(1, sizeof(TProxy));
	assert(h);

	h->outport = htons(atoi(outport));
	h->inport = htons(atoi(inport));
	h->traceport = htons(atoi(traceport));

	h->ined = inputEd;
	h->slogf = slogf;
	h->isDone = 0;

	h->hostIP = inaddr;
	h->remoteIP = outaddr;
	h->traceIP = traceaddr;

	h->hashmap = g_hash_table_new(g_int_hash, g_int_equal);
	h->serverMode = 0;

	prepareToTrace(argc, argv, h);

	/* extract the server hostname from argv if in client mode */
	int isFail = 0;
	isFail = _tproxy_startTProxy(h);

	if(isFail) {
		tproxy_free(h);
		return NULL;
	} else {
		return h;
	}

}

void tproxy_free(TProxy* h) {
	assert(h);

	g_hash_table_destroy(h->hashmap);

	if(h->ined)
		close(h->ined);
	close(h->tracerSocket);

	free(h);
}

char *buildTracePackage(TProxy *h)
{
	/* TODO: Super quick and dirty function. It should be fixed. */

	char *out = malloc(512);
	int len = 0;
	struct timeval tv;

	out[0] = (h->serverMode? 's':'c');
	out[1] = ';';

	/* XXX bug if hostname is > 256 chars */
	gethostname(out + 2, 256);
	len = strlen(out);

	out[len] = ';';

	gettimeofday(&tv, NULL);
	sprintf(out+len+1, "%ld%06ld\n", tv.tv_sec, tv.tv_usec);

	return out;
}

/* Function to trace udp in tor */
void udpTrace(TProxy *h)
{
	char *tracepck = buildTracePackage(h);
	sendto(h->tracerSocket, tracepck, strlen(tracepck), 0,
		(struct sockaddr *) &h->tracerSin, sizeof(struct sockaddr_in));
	return;
}

void closeConnections(TProxy *h, int sd, struct partner_s *partner)
{
	int lsd = sd;

	resetEPOLL(h->ined, sd);
	
	if(close(sd) != 0){
		h->slogf(G_LOG_LEVEL_WARNING, __FUNCTION__,
			"Error in closing connections %d %s", sd, strerror(errno));
	}
	
	h->slogf(G_LOG_LEVEL_WARNING, __FUNCTION__,
			"Closing connections");


	/* Disaster recovery */
	if (partner->bufsize != 0) {
		h->slogf(G_LOG_LEVEL_WARNING, __FUNCTION__,
			 "A free was left out, something nasty is coming! :(");
		free(partner->buf);
	}
	
	g_hash_table_remove(h->hashmap, &lsd);
	h->isDone = 1;
}
/*
 * Called when a new connection is established:
 * create the two connection end-points hashmap entries
 * and set EPOLLIN for them.
 */
void handleNewConnection(TProxy *h, int sd)
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

	if(h->isDone == 1) {
		if (_tproxy_startWriter(h) < 0)
			exit(EXIT_FAILURE);
		h->isDone = 0;
	}

	cin = g_new0(gint, 1);
	*cin = accept(h->asockin, NULL, NULL);

	h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"Reader: accepted connection %d (%s)", *cin,
			strerror(errno));

	udpTrace(h);

	cout = g_new0(gint, 1);
	*cout = socket(AF_INET, SOCK_STREAM, 0);
	if (connect(*cout, &son, sizeof(struct sockaddr_in)) &&
	    errno != EINPROGRESS) {
		/* TODO: This is a very bad practise,
		 * we promise we will do it in the right way :D */
		h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
		         "Error in connect (%s)\n", strerror(errno));
		close(*cin);
		close(*cout);
		return;
	}

	h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
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
		h->slogf(G_LOG_LEVEL_WARNING, __FUNCTION__,
				"Reader, error in epollin (1): %s",
				strerror(errno));

	if(setEPOLLIN(h->ined, *cout))
		h->slogf(G_LOG_LEVEL_WARNING, __FUNCTION__,
				"Reader, error in epollin (2): %s",
				strerror(errno));
}

#define IS_NEW_CONNECTION(sd) (sd == h->asockin)

/*
 * Called when sd can read and set the buffer:
 * reset the input file descriptor and wait for a successfull write.
 */
int handleRead(TProxy *h, int sd, struct partner_s *partner)
{
	partner->buf = (char *)calloc(PAGE_SIZE, sizeof(char));
	if (partner->buf == NULL) {
		h->slogf(G_LOG_LEVEL_WARNING, __FUNCTION__,
			 "Error in calloc\n");
		return -1;
	}

	partner->bufsize = recv(sd, partner->buf, (size_t)PAGE_SIZE, 0);
	
	setEPOLLALL(h->ined, partner->sd);
	resetEPOLL(h->ined, sd);

	if (partner->bufsize <= 0)  {
		return 1;
	}

	partner->buf = (char *)realloc(partner->buf, partner->bufsize);

	if (partner->buf == NULL) {
		h->slogf(G_LOG_LEVEL_WARNING, __FUNCTION__,
			 "Error in realloc %d\n", partner->bufsize);
		return -1;
	}

	h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
		 "Received %d %s\n", partner->bufsize, strerror(errno));

	/* tell epoll we no longer want to watch this socket */
	h->slogf(G_LOG_LEVEL_MESSAGE,
		__FUNCTION__,  "Received a packet: %s\n",
	        /* XXX Pay attention here. Does this rule always apply? */
	        (sd < partner->sd ? "client -> server" : "client <- server"));

	return 0;
}

/*
 * Called when sd can write the buffer:
 * write the buffer and wait for something to read.
 */
int handleWrite(TProxy *h, int sd, int outsd, struct partner_s *me)
{
	ssize_t sentbytes = -1;
	sentbytes = send(sd, me->buf, (size_t)me->bufsize, 0);

	if(sentbytes) {
		h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
		         "successfully sent  message (size %d)", me->bufsize);
	} else {
		h->slogf(G_LOG_LEVEL_WARNING, __FUNCTION__,
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

static void _tproxy_activateTProxy(TProxy* h, int sd, uint32_t events)
{
	struct partner_s *partner;
	int lsd = sd;
	partner = g_hash_table_lookup(h->hashmap, &lsd);

	if (partner == NULL && !IS_NEW_CONNECTION(sd)) {
		h->slogf(G_LOG_LEVEL_DEBUG, __FUNCTION__,
		         "Null pointer in lookup of %d (partner).", sd);
		_exit(EXIT_FAILURE);
	}

	h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "In the activate %d.", sd);

	if(events & EPOLLIN) {
		int res = 0;
		h->slogf(G_LOG_LEVEL_DEBUG, __FUNCTION__, "EPOLLIN is set %d", sd);
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

		if (me == NULL) {
			if (h->isDone == 1) {
				/* If it reaches this point, it means that the other 
			   	end point closed its socket descriptor (connection closed).
			   	Thus this socked descriptor must be closed too. */
				closeConnections(h, sd, partner);
			} else {
				h->slogf(G_LOG_LEVEL_DEBUG, __FUNCTION__,
				         "Null pointer in lookup of %d (me) (connection closed).", sd);
				_exit(EXIT_FAILURE);
			}
		} else {

			h->slogf(G_LOG_LEVEL_DEBUG, __FUNCTION__,
				"EPOLLOUT is set %d", sd);
			handleWrite(h, sd, partner->sd, me);
		}
	}

}

void tproxy_ready(TProxy* h)
{
	assert(h);

	/* collect the events that are ready */
	struct epoll_event epevs[MAX_CLIENTS];
	int nfds = epoll_wait(h->ined, epevs, MAX_CLIENTS, 0);

	h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "TProxy_ready : Successfull");

	if(nfds == -1) {
		h->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
		         "error in epoll_wait");
		return;
	}


	/* activate correct component for every socket thats ready */
	for (int i = 0; i < nfds; i++) {
		int d = epevs[i].data.fd;
		uint32_t e = epevs[i].events;
		_tproxy_activateTProxy(h, d, e);
	}
}

/* return the file descriptor to read from */
int tproxy_getEpollDescriptor(TProxy* h)
{
	assert(h);
	return h->ined;
}

int tproxy_isDone(TProxy* h) {
	assert(h);
	return h->isDone;
}

/*
 * See LICENSE for licensing information
 */

#include "tproxy.h"
#include <signal.h>
#include <stdarg.h>

/* our tproxy code only relies on the log part of shadowlib,
 * so we need to supply that implementation here since this is
 * running outside of shadow. */
static void _mylog(GLogLevelFlags level, const char* functionName, const char* format, ...) {
	va_list variableArguments;
	va_start(variableArguments, format);
	vprintf(format, variableArguments);
	va_end(variableArguments);
	printf("%s", "\n");
}
#define mylog(...) _mylog(G_LOG_LEVEL_INFO, __FUNCTION__, __VA_ARGS__)

void exit_handler(int sn)
{
	printf("exiting, with _exit\n");
	_exit(0);
}

/* this main replaces the tproxy-plugin.c file to run outside of shadow */
int main(int argc, char *argv[]) {


	mylog("Starting TProxy program");
	signal(SIGTERM, exit_handler);
	perror("signal");
	/* create the new state according to user inputs */
	TProxy* tproxyState = tproxy_new(argc, argv, &_mylog);
	if (!tproxyState) {
		mylog("Error initializing new TProxy instance");
		return -1;
	}

	/* now we need to watch all of the tproxy descriptors in our main loop
	 * so we know when we can wait on any of them without blocking. */
	int mainepolld = epoll_create(1);
	if (mainepolld == -1) {
		mylog("Error in main epoll_create");
		close(mainepolld);
		return -1;
	}

	/* tproxy has one main epoll descriptor that watches all of its sockets,
	 * so we now register that descriptor so we can watch for its events */
	struct epoll_event mainevent;
	mainevent.events = EPOLLIN;
	mainevent.data.fd = tproxy_getEpollDescriptor(tproxyState);
	if (!mainevent.data.fd) {
		mylog("Error retrieving tproxy epoll descriptor");
		close(mainepolld);
		return -1;
	}
	epoll_ctl(mainepolld, EPOLL_CTL_ADD, mainevent.data.fd, &mainevent);

	/* main loop - wait for events from the tproxy descriptors */
	struct epoll_event events[100];
	int nReadyFDs;
	mylog("entering main loop to watch descriptors");
	
	while(1) {
		/* wait for some events */
		mylog("waiting for events");
		nReadyFDs = epoll_wait(mainepolld, events, 100, -1);
		if(nReadyFDs == -1) {
			mylog("Error in client epoll_wait");
			return -1;
		}

		/* activate if something is ready */
		mylog("processing event");
		if(nReadyFDs > 0) {
			tproxy_ready(tproxyState);
		}	
	}

	mylog("finished main loop, cleaning up");

	/* de-register the tproxy epoll descriptor */
	mainevent.data.fd = tproxy_getEpollDescriptor(tproxyState);
	if (mainevent.data.fd)
		epoll_ctl(mainepolld, EPOLL_CTL_DEL, mainevent.data.fd, &mainevent);

	/* cleanup and close */
	close(mainepolld);
	/* de-register the tproxy epoll descriptor */
	mainevent.data.fd = tproxy_getEpollDescriptor(tproxyState);
	if (mainevent.data.fd)
		epoll_ctl(mainepolld, EPOLL_CTL_DEL, mainevent.data.fd, &mainevent);

	/* cleanup and close */
	close(mainepolld);
	tproxy_free(tproxyState);

	mylog("exiting cleanly");
	return 0;
}
