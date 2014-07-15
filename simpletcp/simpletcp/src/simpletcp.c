/*
 * See LICENSE for licensing information
 */

#include "simpletcp.h"

#define SERVER 0
#define CLIENT 1

#define HOSTNAME_MAX_LEN 256
#define CLIENT_MSG_SIZE HOSTNAME_MAX_LEN
#define DEFAULT_SLEEP_CONN_TIME 1000 //ms = 1s

#define SOCKS5_INIT 7
#define SOCKS5_METHOD_NEGOTATION_REQ 6
#define SOCKS5_METHOD_NEGOTATION_OK  5
#define SOCKS5_METHOD_NEGOTATION_FAIL 4
#define SOCKS5_CONNECTION_REQ 3
#define SOCKS5_CONNECTION_FAIL 2
#define SOCKS5_CONNECTION_OK 1
#define SOCKS5_DONE 0

static int _simpletcp_startClient(SimpleTCP* h) {
	int res;

	/* create the client socket and get a socket descriptor */
	h->client.sd = socket(AF_INET, SOCK_STREAM , 0);
	if(h->client.sd == -1) {
		h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start client: error in socket");
		return -1;
	}

	/* our client socket address information for connecting to the server */
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = h->client.proxyIP;
	addr.sin_port = h->client.proxyPort;

	/* connect to server. since we are non-blocking, we expect this to 
	   return EINPROGRESS */
	res = connect(h->client.sd, (struct sockaddr *) &addr, sizeof(addr));
	if (res == -1 && errno != EINPROGRESS) {
		h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start client: error in connect");
		return -1;
	}

	/* specify the events to watch for on this socket.
	 * the client wants to know when it can send a simpletcp message. */
	struct epoll_event ev;
	ev.events = EPOLLOUT;
	ev.data.fd = h->client.sd;

	/* start watching the client socket */
	res = epoll_ctl(h->ed, EPOLL_CTL_ADD, h->client.sd, &ev);
	if(res == -1) {
		h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start client: error in epoll_ctl");
		return -1;
	}

	/* success! */
	return 0;
}

static int _simpletcp_startServer(SimpleTCP* h) {
	/* create the socket and get a socket descriptor 
	 * XXX: removed SOCK_NONBLOCK to let it work outside of shadow too
	 */
	h->server.sd = socket(AF_INET, SOCK_STREAM, 0);
	if (h->server.sd == -1) {
		h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start server: error in socket");
		return -1;
	}

	/* setup the socket address info, client has outgoing connection to server */
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = h->server.bindPort;

	/* bind the socket to the server port */
	int res = bind(h->server.sd, (struct sockaddr *)&addr, sizeof(addr));
	if (res == -1) {
		h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start server: error in bind");
		return -1;
	}

	/* set as server socket that will listen for clients */
	res = listen(h->server.sd, 100);
	if (res == -1) {
		h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start server: error in listen");
		return -1;
	}

	/* specify the events to watch for on this socket.
	 * the server wants to know when a client is connecting. */
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = h->server.sd;

	/* start watching the server socket */
	res = epoll_ctl(h->ed, EPOLL_CTL_ADD, h->server.sd, &ev);
	if(res == -1) {
		h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start server: error in epoll_ctl");
		return -1;
	}

	/* success! */
	return 0;
}

in_addr_t _simpletcp_dnsQuery(char *hostname)
{
	struct addrinfo* hostInfo = NULL;
	in_addr_t addr;
	if(strncasecmp(hostname, "none", 4) == 0) {
		addr = htonl(INADDR_NONE);
	}else if(strncasecmp(hostname, "localhost", 9) == 0) {
		addr = htonl(INADDR_LOOPBACK);
	}else if(strncasecmp(hostname, "any", 9) == 0) {
		addr = htonl(INADDR_ANY);
	}else if (getaddrinfo(hostname, NULL, NULL, &hostInfo) >= 0) {
		while (hostInfo->ai_next) hostInfo = hostInfo->ai_next;

		addr = ((struct sockaddr_in *)(hostInfo->ai_addr))->sin_addr.s_addr;

	} else{
		return -1;
	}
	if (hostInfo)
		freeaddrinfo(hostInfo);
	return addr;
}

static void _simpletcp_wakeupClient(void *data)
{
	SimpleTCP *h;
	h = (SimpleTCP *)data;
	h->shdlib->log(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__, 
			"Wake up client! Calling startClient again");
	_simpletcp_startClient(h);
}

static void _simpletcp_sleepCallback(SimpleTCP *h, unsigned int millisecs)
{
	h->shdlib->log(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__,
			"Creating callback with timer %d", millisecs);
	h->shdlib->createCallback(&_simpletcp_wakeupClient, 
			(void *)h, millisecs);
}

/* if option is specified, run as client, else run as server */
static const char* USAGE = "Usage:" 
	"\tsimpletcp client proxyHostname:proxyPort serverHostname:serverPort "
	"nConn [sleep_min,sleep_max](ms)\n"
	"\tsimpletcp server bindHostname:bindPort logFolder\n";

#define _simpletcp_exitUsage(slofg) { \
	shdlib->log(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__, USAGE); \
	return NULL; }


static int _simpletcp_checkClientArgs(int argc, char **argv, int *retargs)
{
	int nconn, sleep_min, sleep_max;
	char *tmp_end, *split1, *split2;

	nconn = sleep_min = sleep_max = 0;

	if (argc < 5){
		return -1;
	
	}
	nconn = (int)strtol(argv[4], &tmp_end, 10);
	if (errno != 0 || nconn > INT_MAX || *tmp_end != '\0'){
		return -1;
	}
	
	if (argc > 5){
		asprintf(&split2, "%s", argv[5]);

		split1 = strsep(&split2, ",");
		if (split1 == NULL || split2 == NULL)
			return -1;

		sleep_min = (int)strtol(split1, &tmp_end, 10);
		if (errno != 0 || sleep_min > INT_MAX || *tmp_end != '\0')
			return -1;
		
		sleep_max = (int)strtol(split2, &tmp_end, 10);
		if (errno != 0 || sleep_max > INT_MAX || *tmp_end != '\0')
			return -1;
		
		/* negative numbers obviously not allowed */
		if (sleep_min < 0 || sleep_max < 0)
			return -1;
	}

	retargs[0] = nconn;
	retargs[1] = sleep_min;
	retargs[2] = sleep_max;

	return 0;
}

SimpleTCP* simpletcp_new(int argc, char* argv[], ShadowFunctionTable *shdlib) 
{
	assert(shdlib);

	int mode, res;
	int cliargs[3];
	char *hostname, *proxy_hostname, *port, *proxy_port;
	char *myhostname, *log_path;
	char *mode_str = argv[1];
	char *tmp;

	/* TODO: Is this random fair enogh? */
	srand((unsigned)time(NULL));

	if (argc < 3)
		_simpletcp_exitUsage(shdlib->log);
			
	if (strncmp(mode_str, "client", 6) == 0) {
		mode = CLIENT;
		res = _simpletcp_checkClientArgs(argc, argv, cliargs);
		if (res == -1)
			_simpletcp_exitUsage(shdlib->log);
		
		asprintf(&tmp, "%s", argv[2]);
		proxy_hostname = strsep(&tmp, ":");
		proxy_port = tmp;

		asprintf(&tmp, "%s", argv[3]);
		hostname = strsep(&tmp, ":");
		port = tmp;
		
		shdlib->log(SHADOW_LOG_LEVEL_DEBUG,__FUNCTION__, 
			"nconn: %d, sleep_min: %d, sleep_max: %d", 
			cliargs[0], cliargs[1], cliargs[2]);

	} else if (strncmp(mode_str, "server", 6) == 0) {
		mode = SERVER;
		if (argc < 4)
			_simpletcp_exitUsage(shdlib->log);
		
		asprintf(&tmp, "%s", argv[2]);
		hostname = strsep(&tmp, ":");
		port = tmp;
		
		asprintf(&log_path, "%s", argv[3]);
	} else {
		_simpletcp_exitUsage(shdlib->log);
	}

	
	myhostname = malloc(HOSTNAME_MAX_LEN);
	
	if (gethostname(myhostname, HOSTNAME_MAX_LEN) == -1){
		if (errno == ENAMETOOLONG) {	
			myhostname[HOSTNAME_MAX_LEN - 1] = 0;
			shdlib->log(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
				"hostname too long, truncated");
		} else {
			shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"gethostname error");
			return NULL;
		}
	}

	shdlib->log(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__, 
		"myhostname %s", myhostname);
	
	/* use epoll to asynchronously watch events for all of our sockets */
	int mainEpollDescriptor = epoll_create(1);
	if(mainEpollDescriptor == -1) {
		shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
			"Error in main epoll_create");
		close(mainEpollDescriptor);
		return NULL;
	}
	
	/* get memory for the new state */
	SimpleTCP* h = calloc(1, sizeof(SimpleTCP));
	assert(h);
	
	h->hostname = myhostname;
	h->ed = mainEpollDescriptor;
	h->shdlib = shdlib;
	h->isDone = 0;

	/* extract the server hostname from argv if in client mode */
	int isFail = 0;
	if (mode == CLIENT) {
		/* client mode */
		h->client.serverIP = _simpletcp_dnsQuery(hostname);
		h->client.serverPort = htons(atoi(port));
		h->client.proxyIP = _simpletcp_dnsQuery(proxy_hostname);
		h->client.proxyPort = htons(atoi(proxy_port));
		h->client.nconn = cliargs[0];
		h->client.sleep_min = cliargs[1];
		h->client.sleep_max = cliargs[2];
		h->client.socks5_status = SOCKS5_INIT;
		
		if (h->client.serverIP == -1 || h->client.proxyIP == -1) {
			shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"Error in _simpletcp_dnsQuery");
			return NULL;
		}
		isFail = _simpletcp_startClient(h);
	} else {
		/* server mode */
		
		/* Prepare the log file */
		asprintf(&(h->server.log_path), "%s/%s", log_path, h->hostname);

		/* Just clean old data if the file exists. */
		FILE *file = fopen(h->server.log_path, "w+");
		if (file == NULL){
			shdlib->log(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
				"Error in fopen: %s", strerror(errno));
			return NULL;
		}
		fclose(file);
		
		h->server.hashtab = g_hash_table_new(g_int_hash, g_int_equal);	
		h->server.bindPort = htons(atoi(port));
		isFail = _simpletcp_startServer(h);
	}

	if(isFail) {
		simpletcp_free(h);
		return NULL;
	} else {
		return h;
	}
}

void simpletcp_free(SimpleTCP* h) {
	assert(h);

	if(h->client.sd)
		close(h->client.sd);
	if(h->ed)
		close(h->ed);

	free(h);
}

/* It prints the client hostname and the connection time into 
 * a log file that has the server hostname as filename. */
static void _simpletcp_logServerConnection(SimpleTCP * h, int sd, 
						char *client_hostname)
{
	int lsd = sd;
	char *logbuf, *server_hostname;
	struct timeval *time = g_hash_table_lookup(h->server.hashtab, &lsd);
	if (time == NULL){
		h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
			"Null pointer in lookup of %d", sd);
		_exit(EXIT_FAILURE);
	}
	
	asprintf(&logbuf, "%s;%ld%06ld\n", h->hostname, 
			time->tv_sec, time->tv_usec);

	int logfd = open(h->server.log_path, 
			O_WRONLY | O_APPEND | O_CREAT, 0775);
	if (logfd == -1){
		h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
			"Error in open %s", strerror(errno));
		_exit(EXIT_FAILURE);
	}
	write(logfd, logbuf, strlen(logbuf));
	close(logfd);
}

static int _simpletcp_socks5Negotation(SimpleTCP *h, int sd, int event)
{
	struct socks5_packet_bare {
		uint8_t ver;
		uint8_t nmethods;
		uint8_t method;
	} sck_pck;

	struct iovec iov[3];

	int res;

	/* Version */
	sck_pck.ver = 0x05;
	/* Number of methods */
	sck_pck.nmethods = 1;
	/* No auth */
	sck_pck.method = 0;

	iov[0].iov_base = &sck_pck.ver;
	iov[0].iov_len = sizeof(sck_pck.ver);

	iov[1].iov_base = &sck_pck.nmethods;
	iov[1].iov_len = sizeof(sck_pck.nmethods);

	iov[2].iov_base = &sck_pck.method;
	iov[2].iov_len = sizeof(sck_pck.method);
	
	if (event == EPOLLIN) {
		res = writev(sd, iov, 3);
		if (res == -1) {
			h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"error in writev %s", strerror(errno));
			return -1;
		}
	} else {
		res = readv(sd, iov, 2);
		if (res == -1) {
			h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"error in readv %s", strerror(errno));
			return -1;
		} 
		
		if (sck_pck.nmethods == 0xff) {
			h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"error in negotation response");
			return -1;
		}
	}

	return 0;
}

static int _simpletcp_socks5ConnRequest(SimpleTCP *h, int sd, int event)
{
	struct socks5_packet_request {
		uint8_t ver;
		uint8_t cmd;
		uint8_t res;
		uint8_t atyp;
		uint32_t dstaddr; /* only ipv4 for test */
		uint16_t dstport;
	} sck_pck_req;
	
	struct iovec iov[6];
	
	int res;

	/* connection request */
	sck_pck_req.ver = 0x5;
	sck_pck_req.cmd = 0x1;
	sck_pck_req.res = 0x00;
	sck_pck_req.atyp = 0x1;
	sck_pck_req.dstaddr = h->client.serverIP;
	sck_pck_req.dstport = h->client.serverPort;

	iov[0].iov_base = &sck_pck_req.ver;
	iov[0].iov_len = sizeof(sck_pck_req.ver);

	iov[1].iov_base = &sck_pck_req.cmd;
	iov[1].iov_len = sizeof(sck_pck_req.cmd);

	iov[2].iov_base = &sck_pck_req.res;
	iov[2].iov_len = sizeof(sck_pck_req.res);

	iov[3].iov_base = &sck_pck_req.atyp;
	iov[3].iov_len = sizeof(sck_pck_req.atyp);

	iov[4].iov_base = &sck_pck_req.dstaddr;
	iov[4].iov_len = sizeof(sck_pck_req.dstaddr);

	iov[5].iov_base = &sck_pck_req.dstport;
	iov[5].iov_len = sizeof(sck_pck_req.dstport);
	
	if (event == EPOLLIN) {
		res = writev(sd, iov, 6);
		if (res == -1) {
			h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"error in writev %s", strerror(errno));
			return -1;
		}
	} else {
		res = readv(sd, iov, 6);
		if (res == -1) {
			h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"error in writev %s", strerror(errno));
			return -1;
		} 
		
		if (sck_pck_req.cmd != 0x00) {
			h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"error in connection request response");
			return -1;
		}
	}

	return 0;
}

static void _simpletcp_clientSendMsg(SimpleTCP *h, int sd)
{
	char message[CLIENT_MSG_SIZE];
	ssize_t numBytes = 0;

	/* prepare the message */
	memset(message, 0, (size_t)CLIENT_MSG_SIZE);
	
	strcpy(message, h->hostname);
	
	/* send the message */
	numBytes = send(sd, message, (size_t)strlen(message), 0);

	/* log result */
	if(numBytes == strlen(message)) {
		h->shdlib->log(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"successfully sent '%s' message", message);
	} else {
		h->shdlib->log(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
			"unable to send message");
	}
}

static void _simpletcp_clientRecvMsg(SimpleTCP *h, int sd)
{
	ssize_t numBytes = 0;
	char message[10];

	/* prepare to accept the message */
	memset(message, 0, (size_t)10);

	numBytes = recv(sd, message, (size_t)6, 0);

	/* log result */
	if(numBytes > 0) {
		h->shdlib->log(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
			"successfully received '%s' message", message);
	} else {
		h->shdlib->log(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
			"unable to receive message");
	}
}

static void _simpletcp_clientWrite(SimpleTCP *h, int sd)
{
	struct epoll_event ev;
	int s5_status = h->client.socks5_status;

	switch(s5_status) {
	case SOCKS5_INIT:
		s5_status = SOCKS5_METHOD_NEGOTATION_REQ;
		_simpletcp_socks5Negotation(h, sd, EPOLLIN);
		break;
	case SOCKS5_METHOD_NEGOTATION_OK:
		s5_status = SOCKS5_CONNECTION_REQ;
		_simpletcp_socks5ConnRequest(h, sd, EPOLLIN);
		break;
	case SOCKS5_CONNECTION_OK:
		s5_status = SOCKS5_DONE;
	case SOCKS5_DONE:
		_simpletcp_clientSendMsg(h, sd);
		break;
	default:
		h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
			"Unexpected or failing Socks5 status: %d", s5_status);
		/* TODO: close the fd and remove from epoll */
		_exit(EXIT_FAILURE);
	}

	h->client.socks5_status = s5_status;
	/* tell epoll we don't care about writing anymore */
	memset(&ev, 0, sizeof(struct epoll_event));
	ev.events = EPOLLIN;
	ev.data.fd = sd;
	epoll_ctl(h->ed, EPOLL_CTL_MOD, sd, &ev);
}

static void _simpletcp_clientRead(SimpleTCP *h, int sd)
{
	struct epoll_event ev;
	struct iovec iov[6];
	int res;

	switch(h->client.socks5_status) {
	case SOCKS5_METHOD_NEGOTATION_REQ:
		res = _simpletcp_socks5Negotation(h, sd, EPOLLOUT);
		if (res == -1)
			h->client.socks5_status = SOCKS5_METHOD_NEGOTATION_FAIL;
		else 
			h->client.socks5_status = SOCKS5_METHOD_NEGOTATION_OK;
		
		/* TODO clean this */
		/* tell epoll we don't care about writing anymore */
		memset(&ev, 0, sizeof(struct epoll_event));
		ev.events = EPOLLOUT;
		ev.data.fd = sd;
		epoll_ctl(h->ed, EPOLL_CTL_MOD, sd, &ev);
		
		break;
	case SOCKS5_CONNECTION_REQ:
		res = _simpletcp_socks5ConnRequest(h, sd, EPOLLOUT);
		if (res == -1)
			h->client.socks5_status = SOCKS5_CONNECTION_FAIL;
		else 
			h->client.socks5_status = SOCKS5_CONNECTION_OK;
		
		/* TODO clean this */
		/* tell epoll we don't care about writing anymore */
		memset(&ev, 0, sizeof(struct epoll_event));
		ev.events = EPOLLOUT;
		ev.data.fd = sd;
		epoll_ctl(h->ed, EPOLL_CTL_MOD, sd, &ev);
		
		break;
	case SOCKS5_DONE:
		/* tell epoll we no longer want to watch this socket */
		epoll_ctl(h->ed, EPOLL_CTL_DEL, sd, NULL);

		close(sd);
		h->client.sd = 0;
		
		/* The client will connect again */ 
		if (h->client.nconn > 0) {
			unsigned int sleep_time = DEFAULT_SLEEP_CONN_TIME;
			int smin, smax;

			h->client.socks5_status = SOCKS5_INIT;
			h->client.nconn--;

			smin = h->client.sleep_min;
			smax = h->client.sleep_max;

			if (smin != 0 || smax != 0)
				sleep_time = rand() % (smax - smin) + smin; 	
			
			_simpletcp_sleepCallback(h, sleep_time);

		} else {
			h->isDone = 1;
		}
		
		break;
	default:
		h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
			"Unexpected or failing socks5 status: %d", 
			h->client.socks5_status);
		_exit(EXIT_FAILURE);
	}
}

static void _simpletcp_serverRead(SimpleTCP *h, int sd)
{
	struct epoll_event ev;
	size_t numBytes = 0;
	char message[CLIENT_MSG_SIZE];
	
	/* prepare to accept the message */
	memset(message, 0, (size_t)CLIENT_MSG_SIZE);

	numBytes = recv(sd, message, (size_t)CLIENT_MSG_SIZE, 0);

	/* log result */
	if(numBytes > 0) {
		h->shdlib->log(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__, 
			"successfully received '%s' message", 
			message);

		_simpletcp_logServerConnection(h, sd, message);

	} else if(numBytes == 0){
		/* client got response and closed */
		/* tell epoll we no longer want to watch this socket */
		epoll_ctl(h->ed, EPOLL_CTL_DEL, sd, NULL);
		close(sd);
		g_hash_table_remove(h->server.hashtab, &sd);
	} else {
		h->shdlib->log(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
			"unable to receive message");
	}

	/* tell epoll we want to write the response now */
	memset(&ev, 0, sizeof(struct epoll_event));
	ev.events = EPOLLOUT;
	ev.data.fd = sd;
	epoll_ctl(h->ed, EPOLL_CTL_MOD, sd, &ev);
}

static void _simpletcp_serverWrite(SimpleTCP *h, int sd)
{
	struct epoll_event ev;
	size_t numBytes = 0;
	char message[10];
	
	/* prepare the response message */
	memset(message, 0, (size_t)10);
	snprintf(message, 10, "%s", "World!");

	/* send the message */
	numBytes = send(sd, message, (size_t)6, 0);

	/* log result */
	if(numBytes == 6) {
		h->shdlib->log(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"successfully sent '%s' message", message);
	} else {
		h->shdlib->log(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__,
				"unable to send message");
	}

	/* now wait until we read 0 for client close event */
	memset(&ev, 0, sizeof(struct epoll_event));
	ev.events = EPOLLIN;
	ev.data.fd = sd;
	epoll_ctl(h->ed, EPOLL_CTL_MOD, sd, &ev);
}

static void _simpletcp_activateClient(SimpleTCP* h, int sd, uint32_t events) {
	assert(h->client.sd == sd);

	if(events & EPOLLOUT) {
		h->shdlib->log(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__, 
			"EPOLLOUT is set");
	}
	if(events & EPOLLIN) {
		h->shdlib->log(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__, 
			"EPOLLIN is set");
	}

	/* to keep things simple, there is explicitly no resilience here.
	 * we allow only one chance to send the message 
	 * and one to receive the response.
	 */
	if(events & EPOLLOUT) {
		/* the kernel can accept data from us,
		 * and we care because we registered EPOLLOUT on sd with epoll
		 */
		_simpletcp_clientWrite(h, sd);
	} else if(events & EPOLLIN) {
		/* there is data available to read from the kernel,
		 * and we care because we registered EPOLLIN on sd with epoll 
		 */
		_simpletcp_clientRead(h, sd);
	}
}

static void _simpletcp_activateServer(SimpleTCP* h, int sd, uint32_t events) 
{
	struct epoll_event ev;
	
	if(events & EPOLLOUT)
		h->shdlib->log(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__, 
			"EPOLLOUT is set");
	if(events & EPOLLIN) 
		h->shdlib->log(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__, 
			"EPOLLIN is set");

	if(sd == h->server.sd) {
		/* data on a listening socket means a new client connection */
		assert(events & EPOLLIN);

		/* accept new connection from a remote client */
		int newClientSD = accept(sd, NULL, NULL);
		gint *sd_key;
		struct timeval *conn_time;

		h->shdlib->log(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__, 
			"ACCEPT: %s", strerror(errno));
		
		/* We have to store the connection time.
		 * We do this using a hashtable. */
		sd_key = g_new0(gint, 1);
		conn_time = g_new0(struct timeval, 1);
		
		*sd_key = newClientSD;
		gettimeofday(conn_time, NULL);
		
		g_hash_table_replace(h->server.hashtab, sd_key, conn_time);
		
		/* now register this new socket so we know when its ready */
		memset(&ev, 0, sizeof(struct epoll_event));
		ev.events = EPOLLIN;
		ev.data.fd = newClientSD;
		epoll_ctl(h->ed, EPOLL_CTL_ADD, newClientSD, &ev);
	} else {
		/* a client is communicating with us over an 
		   existing connection */
		if (events & EPOLLIN)
			_simpletcp_serverRead(h, sd);
		else if (events & EPOLLOUT) 
			_simpletcp_serverWrite(h, sd);
	}
}

void simpletcp_ready(SimpleTCP* h) {
	assert(h);

	/* collect the events that are ready */
	struct epoll_event epevs[10];
	int nfds = epoll_wait(h->ed, epevs, 10, 0);
	if(nfds == -1) {
		h->shdlib->log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"error in epoll_wait");
		return;
	}

	/* activate correct component for every socket thats ready */
	for(int i = 0; i < nfds; i++) {
		int d = epevs[i].data.fd;
		uint32_t e = epevs[i].events;
		if(d == h->client.sd) {
			_simpletcp_activateClient(h, d, e);
		} else {
			_simpletcp_activateServer(h, d, e);
		}
	}
}

int simpletcp_getEpollDescriptor(SimpleTCP* h) {
	assert(h);
	return h->ed;
}

int simpletcp_isDone(SimpleTCP* h) {
	assert(h);
	return h->isDone;
}
