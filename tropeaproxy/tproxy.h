/*
 * See LICENSE for licensing information
 */

#ifndef TProxy_H_
#define TProxy_H_

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

#include <glib.h>

#define OUTGOING 0
#define INCOMING 1

typedef struct _TProxy TProxy;

typedef void (*TProxyLogFunc)(GLogLevelFlags level,
                          const char* functionName,
                          const char* format, ...);

TProxy* tproxy_new(int argc, char* argv[], TProxyLogFunc slogf);
void tproxy_free(TProxy* h);
void tproxy_ready(TProxy* h);
int tproxy_getEpollDescriptor(TProxy* h);
int tproxy_isDone(TProxy* h);
int tproxy_resetAccept(TProxy *h);

#endif /* TProxy_H_ */
