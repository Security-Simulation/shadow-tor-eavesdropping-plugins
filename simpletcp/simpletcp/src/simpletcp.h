/*
 * See LICENSE for licensing information
 */

#ifndef SIMPLETCP_H_
#define SIMPLETCP_H_


#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <linux/ip.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>

#include <shd-library.h>


typedef struct _SimpleTCP SimpleTCP;

/* all state for simpletcp is stored here */
struct _SimpleTCP {
	/* the function we use to log messages
	 * needs level, functionname, and format */
	ShadowLogFunc slogf;
	
	ShadowFunctionTable *shadowlib;

	/* the epoll descriptor to which we will add our sockets.
	 * we use this descriptor with epoll to watch events on our sockets. */
	int ed;

	/* track if our client got a response and we can exit */
	int isDone;

	char *hostname;

	struct {
		int sd;
		int serverPort;
		int nconn;
		int sleep_min;
		int sleep_max;
		in_addr_t serverIP;
	} client;

	struct {
		int sd;
		int bindPort;
		GHashTable *hashtab;
		char *log_path;
	} server;
};

SimpleTCP* simpletcp_new(int argc, char* argv[], 
					ShadowFunctionTable *shadowlib, ShadowLogFunc slogf);
void simpletcp_free(SimpleTCP* h);
void simpletcp_ready(SimpleTCP* h);
int simpletcp_getEpollDescriptor(SimpleTCP* h);
int simpletcp_isDone(SimpleTCP* h);

#endif /* SIMPLETCP_H_ */
