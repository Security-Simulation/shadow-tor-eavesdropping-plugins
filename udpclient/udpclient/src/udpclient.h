/*
 * See LICENSE for licensing information
 */

#ifndef UDPClient_H_
#define UDPClient_H_

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
#include <errno.h>

#include <stdarg.h>

#include <shd-library.h>

#define OUTGOING 0
#define INCOMING 1

typedef struct _UDPClient UDPClient;

UDPClient* udpclient_new(int argc, char* argv[], ShadowLogFunc slogf);
void udpclient_free(UDPClient* h);
void udpclient_ready(UDPClient* h);
int udpclient_getEpollDescriptor(UDPClient* h);
int udpclient_isDone(UDPClient* h);

#endif /* UDPClient_H_ */
