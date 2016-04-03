/*
 * See LICENSE for licensing information
 */

#define _GNU_SOURCE
#include "loggerserver.h"

#define PAGE_SIZE 4096

/* all state for loggerserver is stored here */
struct _LoggerServer {
	/* the function we use to log messages
	 * needs level, functionname, and format */
	LoggerServerLogFunc slogf;

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

	char *log_path;
	
	in_addr_t hostIP;	
};

/* if option is specified, run loggerserver client, else run loggerserver server */
static const char* USAGE = "USAGE: loggerserver hostname_bind:LISTEN_PORT filename\n";

/* At the BEGINNING this is the proxy reader end, whose that 
   communicates with the real client (this would be a writer too then) */
static int _loggerserver_startReader(LoggerServer* h) 
{
	/* create the socket and get a socket descriptor */
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_port = h->inport,
	};

	sin.sin_addr.s_addr = h->hostIP;

	h->asockin = socket(AF_INET, SOCK_DGRAM, 0);
	if (h->asockin == -1){
		h->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
		         "unable to start reader: error in socket");
		return -1;
	}
	h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
	         "Reader: create socket %d", h->asockin);

	if (bind(h->asockin, (struct sockaddr *)&sin, sizeof(struct sockaddr_in))) {
		h->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
		         "unable to start reader: error in bind (%s)",
		         strerror(errno));
		return -1;
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = h->asockin;
	if (epoll_ctl(h->ined, EPOLL_CTL_ADD, h->asockin, &ev) == -1) {
		h->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
		         "unable to start reader: error in epoll_ctl (%s)",
		         strerror(errno));
		return -1;
	}

	h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "Reader started.");

	return 0;
}

static int _loggerserver_startLoggerServer(LoggerServer *h)
{
	return _loggerserver_startReader(h);
}

LoggerServer* loggerserver_new(int argc, char* argv[], LoggerServerLogFunc slogf)
{
	assert(slogf);

	struct addrinfo* hostInfo = NULL;
	in_addr_t inaddr = 0;
	char *hostname_bind;
	char *inport;
	char *tmp;


	if(argc != 3) {
		slogf(G_LOG_LEVEL_WARNING, __FUNCTION__, USAGE);
		return NULL;
	}

	asprintf(&tmp, "%s", argv[1]);
	hostname_bind = strsep(&tmp, ":");
	inport = tmp;

	/* use epoll to asynchronously watch events for all of our sockets */

	int inputEd = epoll_create(1);
	if(inputEd == -1) {
		slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__, 
		      "Error in main epoll_create");
		close(inputEd);
		return NULL;
	}
	/* DNS query */
	/* get the address in network order */
	if(strncasecmp(hostname_bind, "none", 4) == 0) {
		inaddr = htonl(INADDR_NONE);
	} else if(strncasecmp(hostname_bind, "localhost", 9) == 0) {
		inaddr = htonl(INADDR_LOOPBACK);
	} else if(strncasecmp(hostname_bind, "any", 9) == 0) {
		inaddr = htonl(INADDR_ANY);
	} else if (getaddrinfo(hostname_bind, NULL, NULL, &hostInfo) >= 0) {		
		while (hostInfo->ai_next) hostInfo = hostInfo->ai_next;

		inaddr = ((struct sockaddr_in *)(hostInfo->ai_addr))->sin_addr.s_addr;
	
	} else {
		slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
		      "Error in main getaddrinfo (%s)", strerror(errno));
		close(inputEd);
		return NULL;
	}
	
	memset(&hostInfo, 0, sizeof(hostInfo));	

	slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "%s inaddr (%s)",
	      hostname_bind, inet_ntoa(*(struct in_addr *)&inaddr));

	/* get memory for the new state */
	LoggerServer* h = calloc(1, sizeof(LoggerServer));
	assert(h);

	h->inport = htons(atoi(inport));
	h->ined = inputEd;
	h->slogf = slogf;
	h->isDone = 0;
	h->endread = 0;
	h->good_data = 0;
	h->hostIP = inaddr;

	h->log_path = malloc(strlen(argv[2]));
	strcpy(h->log_path, argv[2]);

	/* Just clean old data if the file exists. */
#if 0
	/* This seems broken. */
	FILE *file = fopen(h->log_path, "w+");
	fclose(file);
#else
	int nfd = open(h->log_path, O_RDWR| O_CREAT, 0775);
	close(nfd);
#endif

	if (hostInfo)
		freeaddrinfo(hostInfo);

	/* extract the server hostname from argv if in client mode */
	int isFail = 0;
	isFail = _loggerserver_startLoggerServer(h);

	if (isFail) {
		loggerserver_free(h);
		return NULL;
	} else {
		return h;
	}

}

void loggerserver_free(LoggerServer* h)
{
	assert(h);

	if(h->ined)
		close(h->ined);

	free(h);
}


static void _loggerserver_activate(LoggerServer* h, int sd, uint32_t events)
{
	h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, 
	         "In the activate %x.", events);

	if(events & EPOLLIN) {
		char buf[4096];
		h->slogf(G_LOG_LEVEL_DEBUG, __FUNCTION__,
		         "EPOLLIN is set %d", sd);
		h->endread = 0;
		h->good_data = recv(sd, buf, (size_t)PAGE_SIZE, 0);
		h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
			 "received %d %s ", h->good_data, strerror(errno));
		/* log result */
		if(h->good_data > 0) {
			h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
			         "successfully received a message:");
			int logfd = open(h->log_path, O_WRONLY | O_APPEND | O_CREAT, 0775);
			write(logfd, buf, h->good_data);
			
			/* shadow 1.11 message checks */
			printf("open: %s\n", strerror(errno));
			close(logfd);
			/* Message Processing */
		}
	}
}

void loggerserver_ready(LoggerServer* h) 
{
	assert(h);

	/* collect the events that are ready */
	struct epoll_event epevs[10];
	int nfds = epoll_wait(h->ined, epevs, 10, 0);
	
	h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "As_ready : Successfull");

	if(nfds == -1) {
		h->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__, 
		         "error in epoll_wait");
		return;
	}

	/* activate correct component for every socket thats ready */
	for(int i = 0; i < nfds; i++) {
		int d = epevs[i].data.fd;
		uint32_t e = epevs[i].events;
		/* if (e) */
			_loggerserver_activate(h, d, e);
	}
}

/* return the file descriptor to read from */
int loggerserver_getEpollDescriptor(LoggerServer* h)
{
	assert(h);
	return h->ined;
}

int loggerserver_isDone(LoggerServer* h)
{
	assert(h);
	return h->isDone;
}

int loggerserver_resetAccept(LoggerServer* h)
{
	h->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "resetting the whole thing");
	
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = h->asockin;
	if (epoll_ctl(h->ined, EPOLL_CTL_ADD, h->asockin, &ev) == -1){
		h->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
		         "unable to reset everything: error in epoll_ctl_add (%s)",
		         strerror(errno));
		return -1;
	}
	
	return 0;
}
/*
 * See LICENSE for licensing information
 */

#include "loggerserver.h"
#include <signal.h>
#include <stdarg.h>

/* our loggerserver code only relies on the log part of shadowlib,
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

/* this main replaces the loggerserver-plugin.c file to run outside of shadow */
int main(int argc, char *argv[]) {
	mylog("Starting LoggerServer program");
	signal(SIGTERM, exit_handler);
	perror("signal");
	/* create the new state according to user inputs */
	LoggerServer* loggerserverState = loggerserver_new(argc, argv, &_mylog);
	if(!loggerserverState) {
		mylog("Error initializing new LoggerServer instance");
		return -1;
	}

	perror("loggerstart");

	/* now we need to watch all of the as descriptors in our main loop
	 * so we know when we can wait on any of them without blocking. */
	int mainepolld = epoll_create(1);
	if(mainepolld == -1) {
		mylog("Error in main epoll_create");
		close(mainepolld);
		return -1;
	}

	/* loggerserver has one main epoll descriptor that watches all of its sockets,
	 * so we now register that descriptor so we can watch for its events */
	struct epoll_event mainevent;
	mainevent.events = EPOLLIN;
	mainevent.data.fd = loggerserver_getEpollDescriptor(loggerserverState);
	if(!mainevent.data.fd) {
		mylog("Error retrieving loggerserver epoll descriptor");
		close(mainepolld);
		return -1;
	}
	epoll_ctl(mainepolld, EPOLL_CTL_ADD, mainevent.data.fd, &mainevent);

	/* main loop - wait for events from the loggerserver descriptors */
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
			loggerserver_ready(loggerserverState);
		}

		/* break out if loggerserver is done */
		if(loggerserver_isDone(loggerserverState)) {
			loggerserver_resetAccept(loggerserverState);
			/*break;*/
		}
	}

	mylog("finished main loop, cleaning up");

	/* de-register the loggerserver epoll descriptor */
	mainevent.data.fd = loggerserver_getEpollDescriptor(loggerserverState);
	if (mainevent.data.fd)
		epoll_ctl(mainepolld, EPOLL_CTL_DEL, mainevent.data.fd, &mainevent);

	/* cleanup and close */
	close(mainepolld);
	/* de-register the loggerserver epoll descriptor */
	mainevent.data.fd = loggerserver_getEpollDescriptor(loggerserverState);
	if (mainevent.data.fd)
		epoll_ctl(mainepolld, EPOLL_CTL_DEL, mainevent.data.fd, &mainevent);

	/* cleanup and close */
	close(mainepolld);
	loggerserver_free(loggerserverState);

	mylog("exiting cleanly");
	return 0;
}
