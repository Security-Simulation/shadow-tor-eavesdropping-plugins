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
#include <errno.h>

#include <shd-library.h>

typedef struct _SimpleTCP SimpleTCP;

SimpleTCP* simpletcp_new(int argc, char* argv[], ShadowLogFunc slogf);
void simpletcp_free(SimpleTCP* h);
void simpletcp_ready(SimpleTCP* h);
int simpletcp_getEpollDescriptor(SimpleTCP* h);
int simpletcp_isDone(SimpleTCP* h);

#endif /* SIMPLETCP_H_ */
