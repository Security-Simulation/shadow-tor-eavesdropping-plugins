/*
 * See LICENSE for licensing information
 */

#ifndef AS_H_
#define AS_H_

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

typedef struct _AS AS;

AS* as_new(int argc, char* argv[], ShadowLogFunc slogf);
void as_free(AS* h);
void as_ready(AS* h);
int as_getEpollDescriptor(AS* h);
int as_isDone(AS* h);
int as_resetAccept(AS *h);

#endif /* AS_H_ */
