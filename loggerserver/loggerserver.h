/*
 * See LICENSE for licensing information
 */

#ifndef LoggerServer_H_
#define LoggerServer_H_

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

#include <linux/ip.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <unistd.h>
#include <glib.h>

#define OUTGOING 0
#define INCOMING 1

typedef struct _LoggerServer LoggerServer;

typedef void (*LoggerServerLogFunc)(GLogLevelFlags level,
                          const char* functionName,
                          const char* format, ...);

LoggerServer* loggerserver_new(int argc, char* argv[], LoggerServerLogFunc slogf);
void loggerserver_free(LoggerServer* h);
void loggerserver_ready(LoggerServer* h);
int loggerserver_getEpollDescriptor(LoggerServer* h);
int loggerserver_isDone(LoggerServer* h);
int loggerserver_resetAccept(LoggerServer *h);

#endif /* LoggerServer_H_ */
