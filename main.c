#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netgraph.h>
#include <sys/socket.h>
#include <netgraph/ng_ksocket.h>

#define SRC_IP "0.0.0.0"
#define SRC_PORT "8000"

static int csock, dsock;
static char ksock_name[NG_NODESIZ];

int CreateNgSock(void);
int StartServer(void);
int ListenNgKsocket();
int CreateNgKsocket();
int sendAccept(char path[NG_PATHSIZ]);
int SetKsocketKeepAlive (char path[NG_PATHSIZ]);


int main( int argc, char *argv[] ) {
    printf("Starting http server...\n");
    CreateNgSock();
    StartServer();

}

int CreateNgSock() {
    char name[NG_PATHSIZ];
    memset(name, 0, sizeof(name));
    sprintf(name, "%s-%d", "http_server", getpid());
    if ( NgMkSockNode(name, &csock, &dsock) < 0) {
        printf("Error has occured while creating NgSocket: %s\n", strerror(errno));
        exit(1);
    }
}

int StartServer() {
    if ( CreateNgKsocket() < 0 ) {
        return -1;
    }

    ListenNgKsocket();
}

int CreateNgKsocket() {
    char path[NG_PATHSIZ];
    struct ngm_mkpeer arg;
    
    sprintf(arg.type, "ksocket");
    sprintf(arg.peerhook, "inet/stream/tcp");
    sprintf(arg.ourhook, "delusion");
    
    memset(path, 0, sizeof(path));
    sprintf(path, ".:");
    if (NgSendMsg(csock, path, NGM_GENERIC_COOKIE, NGM_MKPEER, &arg, sizeof(arg)) < 0 ) {
        printf("%s(): Error sending message: %s", __func__, strerror(errno));
        return -1;
    }
    sprintf(path, ".:%s", arg.ourhook);
    memset( ksock_name, 0, sizeof(ksock_name) );
    sprintf(ksock_name, "http-ksocket-%d", getpid());
    if ( NgNameNode(csock, path, ksock_name, getpid()) < 0 ) {
        printf("%s: error naming node : %s", __func__, strerror(errno));
        return -1;
    }

    /* make kscoket bind and listen state */
    struct sockaddr_in source;
    source.sin_family = AF_INET;
    inet_aton(SRC_IP, &source.sin_addr);
    source.sin_port = htons(atoi(SRC_PORT));
    source.sin_len = sizeof(struct sockaddr_in);
    sprintf(path, "%s:", ksock_name);
    NgSetDebug(3);
    if ( NgSendMsg(csock, path, NGM_KSOCKET_COOKIE, NGM_KSOCKET_BIND,  &source, sizeof(source)) < 0 ) {
        printf("%s(): Error sending ksocket bind: %s\n", __func__, strerror(errno));
        return -1;
    }
    /* Listening ksocket */
    int lst = 1024;
    if ( NgSendMsg(csock, path, NGM_KSOCKET_COOKIE, NGM_KSOCKET_LISTEN,  &lst, sizeof(lst)) < 0 ) {
        printf("%s(): Error sending ksocket bind: %s\n", __func__, strerror(errno));
        return -1;
    }
    int token;
    token = NgSendMsg(csock, path, NGM_KSOCKET_COOKIE, NGM_KSOCKET_ACCEPT, NULL, 0);
    if ( token < 0 && errno != EINPROGRESS && errno != EALREADY ) {
        printf("%s(): Accept Failed %s\n", __func__, strerror(errno));
        return -1;
    }
    SetKsocketKeepAlive(ksock_name);
    return token;
}

int ListenNgKsocket() {
    char path[NG_PATHSIZ];
    int token;
    
    struct ng_mesg *m;
    struct accept_response {
        int node_id;
        struct sockaddr_in client;
    } *resp;
    
    memset(path, 0, sizeof(path));
    printf("%s serving connections\n", __func__); 
    for (;;) {
        if ( NgAllocRecvMsg(csock, &m, path) < 0 ) {
            printf("%s(): Error receiving message : %s\n", __func__, strerror(errno));
            return -1;
        }
        resp = (struct accept_response *)m->data;
        printf("new client nod = %s token = %d arglen = %d\n", path, m->header.token, m->header.arglen);
        printf("client_node_id = %08X client_address = %s:%d\n", resp->node_id, 
                inet_ntoa(resp->client.sin_addr), 
                htons(resp->client.sin_port)
        );
        sendAccept(ksock_name);
        free(m);
    }
}

int sendAccept(char path[NG_PATHSIZ]) {
    uint32_t token;
    if ( path[strlen(path) - 1] != ':' ) 
        sprintf(path, "%s:", path);
    token = NgSendMsg(csock, path, NGM_KSOCKET_COOKIE, NGM_KSOCKET_ACCEPT,
    NULL, 0);
    if ((int) token < 0 && errno != EINPROGRESS && errno != EALREADY) {
        printf("%s(): Error has occured : %s\n" ,__func__, strerror(errno));
    }
    return token;
}

int SetKsocketKeepAlive (char path[NG_PATHSIZ]) {
	union {
		u_char buf[sizeof(struct ng_ksocket_sockopt) + sizeof(int)];
		struct ng_ksocket_sockopt sockopt;
	} sockopt_buf;
	struct ng_ksocket_sockopt * const sockopt = &sockopt_buf.sockopt;
	int one = 1;

    if ( path[strlen(path) - 1] != ':' ) 
        sprintf(path, "%s:", path);
	// setsockopt resolve TIME_WAIT problem
	// setsockopt(fd,SOL_SOCKET,SO_REUSEPORT,&one,sizeof(int)) < 0)
	memset(&sockopt_buf, 0, sizeof(sockopt_buf));

	sockopt->level = SOL_SOCKET;
	sockopt->name = SO_KEEPALIVE;
	memcpy(sockopt->value, &one, sizeof(int));
	if (NgSendMsg(csock, path, NGM_KSOCKET_COOKIE, NGM_KSOCKET_SETOPT,
			sockopt, sizeof(sockopt_buf)) == -1) {
		fprintf(stderr, "%s(): Sockopt set failed : %s\n", __FUNCTION__,
				strerror(errno));
		return -1;
	}
	return 1;
}


