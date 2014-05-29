/*
 * See LICENSE for licensing information
 */

#include "as.h"
#include <signal.h>
#include <stdarg.h>

/* our as code only relies on the log part of shadowlib,
 * so we need to supply that implementation here since this is
 * running outside of shadow. */
static void _mylog(ShadowLogLevel level, const char* functionName, const char* format, ...) {
	va_list variableArguments;
	va_start(variableArguments, format);
	vprintf(format, variableArguments);
	va_end(variableArguments);
	printf("%s", "\n");
}
#define mylog(...) _mylog(SHADOW_LOG_LEVEL_INFO, __FUNCTION__, __VA_ARGS__)

void exit_handler(int sn)
{
	printf("exiting, with _exit\n");
	_exit(0);
}

/* this main replaces the as-plugin.c file to run outside of shadow */
int main(int argc, char *argv[]) {


	mylog("Starting AS program");
	signal(SIGTERM, exit_handler);
	perror("signal");
	/* create the new state according to user inputs */
	AS* asState = as_new(argc, argv, &_mylog);
	if(!asState) {
		mylog("Error initializing new AS instance");
		return -1;
	}

	/* now we need to watch all of the as descriptors in our main loop
	 * so we know when we can wait on any of them without blocking. */
	int mainepolld = epoll_create(1);
	if(mainepolld == -1) {
		mylog("Error in main epoll_create");
		close(mainepolld);
		return -1;
	}

	/* as has one main epoll descriptor that watches all of its sockets,
	 * so we now register that descriptor so we can watch for its events */
	struct epoll_event mainevent;
	mainevent.events = EPOLLIN;
	mainevent.data.fd = as_getEpollDescriptor(asState);
	if(!mainevent.data.fd) {
		mylog("Error retrieving as epoll descriptor");
		close(mainepolld);
		return -1;
	}
	epoll_ctl(mainepolld, EPOLL_CTL_ADD, mainevent.data.fd, &mainevent);

	/* main loop - wait for events from the as descriptors */
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
			as_ready(asState);
		}	
	}

	mylog("finished main loop, cleaning up");

	/* de-register the as epoll descriptor */
	mainevent.data.fd = as_getEpollDescriptor(asState);
	if(mainevent.data.fd) {
		epoll_ctl(mainepolld, EPOLL_CTL_DEL, mainevent.data.fd, &mainevent);
	}

	/* cleanup and close */
	close(mainepolld);
	/* de-register the as epoll descriptor */
	mainevent.data.fd = as_getEpollDescriptor(asState);
	if(mainevent.data.fd) {
		epoll_ctl(mainepolld, EPOLL_CTL_DEL, mainevent.data.fd, &mainevent);
	}

	/* cleanup and close */
	close(mainepolld);
	as_free(asState);

	mylog("exiting cleanly");
	return 0;
}
