/*
 * See LICENSE for licensing information
 */

#ifndef Analyzer_H_
#define Analyzer_H_

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
#include <linux/ip.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <unistd.h>

#define OUTGOING 0
#define INCOMING 1

typedef struct _Analyzer Analyzer;

Analyzer* analyzer_new(int argc, char* argv[], ShadowLogFunc slogf);
void analyzer_free(Analyzer* h);
void analyzer_ready(Analyzer* h);
int analyzer_getEpollDescriptor(Analyzer* h);
int analyzer_isDone(Analyzer* h);
int analyzer_resetAccept(Analyzer *h);

#endif /* Analyzer_H_ */
