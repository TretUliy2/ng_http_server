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

int CreateNgSock(void);
int StartServer(void);
int ListenNgKsocket();
int CreateNgKsocket();

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
    CreateNgKsocket();
    ListenNgKsocket();
}

int CreateNgKsocket() {
    char path[NG_PATHSIZ];
    char ksock_name[NG_NODESIZ];
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
        printf("%s(): Error sending ksocket bind: %s", __func__, strerror(errno));
        return -1;
    }
    /* Listening ksocket */
    int lst = 1024;
    if ( NgSendMsg(csock, path, NGM_KSOCKET_COOKIE, NGM_KSOCKET_LISTEN,  &lst, sizeof(lst)) < 0 ) {
        printf("%s(): Error sending ksocket bind: %s", __func__, strerror(errno));
        return -1;
    }
    int token;
    token = NgSendMsg(srv_csock, path, NGM_KSOCKET_COOKIE, NGM_KSOCKET_ACCEPT, NULL, 0);
    if ( token < 0 && errno != EINPROGRESS && errno != EALREADY ) {
        printf("%s(): Accept Failed %s", __func__, strerror(errno));
        return -1;
    }
    return token;
}

int ListenNgKsocket() {
    struct ng_mesg *m;
    char path[NG_PATHSIZ];
    int token;
    memset(path, 0, sizeof(path));
    
    for (;;) {
        if ( NgAllocRecvMsg(csock, &m, path) < 0 ) {
            printf("%s(): Error receiving message : %s", __func__, strerror(errno));
            return -1;
        }
        printf("new client nod = %s", path);
        
    }
}
