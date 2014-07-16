/*
 * See LICENSE for licensing information
 */

#ifndef SIMPLETCP_H_
#define SIMPLETCP_H_


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
	/* The shadow libraries, it points to all the available functions */
	ShadowFunctionTable *shdlib;

	/* the epoll descriptor to which we will add our sockets.
	 * we use this descriptor with epoll to watch events on our sockets. */
	int ed;

	/* track if our client got a response and we can exit */
	int isDone;

	char *hostname;

	struct {
		int sd;
		int nconn;
		int sleep_min;
		int sleep_max;
		int serverPort;
		int proxyPort;	
		int socks5_status;
		in_addr_t serverIP;
		in_addr_t proxyIP;
	} client;

	struct {
		int sd;
		int bindPort;
		GHashTable *hashtab;
		char *log_path;
	} server;
};

SimpleTCP* simpletcp_new(int argc, char* argv[], ShadowFunctionTable *shdlib);
void simpletcp_free(SimpleTCP* h);
void simpletcp_ready(SimpleTCP* h);
int simpletcp_getEpollDescriptor(SimpleTCP* h);
int simpletcp_isDone(SimpleTCP* h);

#endif /* SIMPLETCP_H_ */
