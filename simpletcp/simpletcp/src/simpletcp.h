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
#include <shd-library.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>

typedef struct _SimpleTCP SimpleTCP;

SimpleTCP* simpletcp_new(int argc, char* argv[], ShadowLogFunc slogf);
void simpletcp_free(SimpleTCP* h);
void simpletcp_ready(SimpleTCP* h);
int simpletcp_getEpollDescriptor(SimpleTCP* h);
int simpletcp_isDone(SimpleTCP* h);

#endif /* SIMPLETCP_H_ */
