/*
 * See LICENSE for licensing information
 */

#ifndef SIMPLESERVER_H_
#define SIMPLESERVER_H_

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

#include <glib.h>

#define OUTGOING 0
#define INCOMING 1

typedef struct _SimpleServer SimpleServer;

SimpleServer* simpleserver_new(int argc, char* argv[], ShadowLogFunc slogf);
void simpleserver_free(SimpleServer* h);
void simpleserver_ready(SimpleServer* h);
int simpleserver_getEpollDescriptor(SimpleServer* h);
int simpleserver_isDone(SimpleServer* h);
int simpleserver_resetAccept(SimpleServer *h);

#endif /* SIMPLESERVER_H_ */
