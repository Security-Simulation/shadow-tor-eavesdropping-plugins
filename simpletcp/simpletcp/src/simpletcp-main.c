/*
 * See LICENSE for licensing information
 */

#include "simpletcp.h" 

void main_log(ShadowLogLevel level, const char* functionName, 
		const char* format, ...) 
{
	va_list variableArguments;
	va_start(variableArguments, format);
	vprintf(format, variableArguments);
	va_end(variableArguments);
	printf("%s", "\n");
}

#define mylog(...) main_log(SHADOW_LOG_LEVEL_INFO, __FUNCTION__, __VA_ARGS__)

void main_createCallback(ShadowPluginCallbackFunc callback, void *data, 
		unsigned int millisecondsDelay) 
{
	usleep(millisecondsDelay * 1000);
	callback(data);
}

ShadowFunctionTable main_functionTable = {
	NULL,
	&main_log,
	&main_createCallback,
	NULL,
};

/* this main replaces the simpletcp-plugin.c file to run outside of shadow */
int main(int argc, char *argv[]) {
	mylog("Starting SimpleTCP program");

	/* create the new state according to user inputs */
	//TODO: pass the shadowlib in some way
	SimpleTCP* simpletcpState = simpletcp_new(argc, argv, 
			&main_functionTable);
	if(!simpletcpState) {
		mylog("Error initializing new SimpleTCP instance");
		return -1;
	}

	/* now we need to watch all of the simpletcp descriptors in our main loop
	 * so we know when we can wait on any of them without blocking. */
	int mainepolld = epoll_create(1);
	if(mainepolld == -1) {
		mylog("Error in main epoll_create");
		close(mainepolld);
		return -1;
	}

	/* simpletcp has one main epoll descriptor that watches all of its sockets,
	 * so we now register that descriptor so we can watch for its events */
	struct epoll_event mainevent;
	mainevent.events = EPOLLIN|EPOLLOUT;
	mainevent.data.fd = simpletcp_getEpollDescriptor(simpletcpState);
	if(!mainevent.data.fd) {
		mylog("Error retrieving simpletcp epoll descriptor");
		close(mainepolld);
		return -1;
	}
	epoll_ctl(mainepolld, EPOLL_CTL_ADD, mainevent.data.fd, &mainevent);

	/* main loop - wait for events from the simpletcp descriptors */
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
			simpletcp_ready(simpletcpState);
		}

		/* break out if simpletcp is done */
		if(simpletcp_isDone(simpletcpState)) {
			break;
		}
	}

	mylog("finished main loop, cleaning up");

	/* de-register the simpletcp epoll descriptor */
	mainevent.data.fd = simpletcp_getEpollDescriptor(simpletcpState);
	if(mainevent.data.fd) {
		epoll_ctl(mainepolld, EPOLL_CTL_DEL, mainevent.data.fd, &mainevent);
	}

	/* cleanup and close */
	close(mainepolld);
	simpletcp_free(simpletcpState);

	mylog("exiting cleanly");

	return 0;
}
