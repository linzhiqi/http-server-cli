#include <sys/socket.h>
#include <strings.h>
#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <signal.h>
#include <errno.h>
#include "logutil.h"

#define LISTENQ 10

/*
 *host - tageted host name or ip address, support both ipv4 and ipv6
 *serv - service name or port number
 *return - the socket file handler	
 */
int tcp_connect(const char *host, const char *serv)
{
    int				sockfd, n;
    const char * addrstr;
    char buf[128];
    struct addrinfo	hints, *res, *ressave;

    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ( (n = getaddrinfo(host, serv, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo erro for server:%s service:%s %s!\n",
        host, serv, gai_strerror(n));
        return -1;
    }
    ressave = res;

    do {
        struct sockaddr_in *sin=(struct sockaddr_in *)res->ai_addr;
        addrstr=inet_ntop(res->ai_family, &(sin->sin_addr),buf, sizeof(buf));
        printf("Address is %s\n", addrstr);

        sockfd = socket(res->ai_family, res->ai_socktype,
                    res->ai_protocol);
        if (sockfd < 0)
            continue;	/* ignore this one */

        if ((n=connect(sockfd, res->ai_addr, res->ai_addrlen)) == 0)
            break;		/* success */
	
        close(sockfd);	/* ignore this one */
    } while ( (res = res->ai_next) != NULL);

    if (res == NULL) {	/* errno set from final connect() */
        fprintf(stderr, "tcp_connect:no working addrinfo for server:%s  service:%s\n", host, serv);
        sockfd = -1;
        perror("tcp_connect() error");
        return -1;
    }

    freeaddrinfo(ressave);

    return(sockfd);
}

/*
 * write the first len of bytes in buf to fd. It writes again if not all len of bytes are written.
 * error can happen, in this case, it returns how many bytes have been written and shows error info.
 */
ssize_t writen(int fd, const char *buf, size_t len){
    int cnt, n, total;
    const char * ptr=buf;
    cnt = len;
    while((n=write(fd,ptr,cnt))>=0)
    {
        write(1,">",1);
	if(n<cnt)
	{
	    ptr+=n;
            total+=n;
	    cnt-=n;
	}else{
	    return len;
	}
    }
    if(errno==EPIPE)
        perror("writen get EPIPE");
    else perror("writen error");
    return total;
}

ssize_t writewithtimeout(int sockfd, const void *buf, size_t count, int sec){
    fd_set wset;
    struct timeval tv;
    int maxfdp1=sockfd+1;

    FD_ZERO(&wset);
    FD_SET(sockfd, &wset);
    tv.tv_sec = sec;
    tv.tv_usec = 0;
    if(select(maxfdp1, NULL, &wset, NULL, &tv)==0){
        printf("writewithtimeout(): time out!\n");
        return -2;
    }else{
        return write(sockfd, buf, count);
    }
}

ssize_t writenwithtimeout(int fd, const char *buf, size_t len, int sec){
    int cnt, n, total;
    const char * ptr=buf;
    cnt = len;
    while((n=writewithtimeout(fd,ptr,cnt,sec))>=0)
    {
        write(1,">",1);
	if(n<cnt)
	{
	    ptr+=n;
            total+=n;
	    cnt-=n;
	}else{
	    return len;
	}
    }
    if(n==-2){
        printf("writenwithtimeout(): time out!\n");
        return -2;
    }
    if(errno==EPIPE)
        perror("writen get EPIPE");
    else perror("writen error");
    return total;
}



ssize_t readwithtimeout(int sockfd, void *buf, size_t count, int sec){
    fd_set rset;
    struct timeval tv;
    int maxfdp1=sockfd+1;

    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    tv.tv_sec = sec;
    tv.tv_usec = 0;
    if(select(maxfdp1, &rset, NULL, NULL, &tv)==0){
        printf("readwithtimeout(): time out!\n");
        return -2;
    }else{
        return read(sockfd, buf, count);
    }
}

/*
 * copied from course example
 */
int tcp_listen(const char *host, const char *serv, socklen_t *addrlenp)
{
        int                             listenfd, n;
        const int               on = 1;
        struct addrinfo hints, *res, *ressave;

        bzero(&hints, sizeof(struct addrinfo));
        hints.ai_flags = AI_PASSIVE;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if ( (n = getaddrinfo(host, serv, &hints, &res)) != 0) {
                log_error("tcp_listen error for %s, %s: %s",
                                 host, serv, gai_strerror(n));
		return -1;
	}
        ressave = res;

        do {
                listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
                if (listenfd < 0)
                        continue;               /* error, try next one */

                setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
                if (bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
                        break;                  /* success */

                close(listenfd);        /* bind error, close and try next one */
        } while ( (res = res->ai_next) != NULL);

        if (res == NULL) {        /* errno from final socket() or bind() */
                fprintf(stderr, "tcp_listen error for %s, %s", host, serv);
		return -1;
	}

        if (listen(listenfd, LISTENQ) < 0) {
		perror("listen");
		return -1;
	}

        if (addrlenp)
                *addrlenp = res->ai_addrlen;    /* return size of protocol address */

        freeaddrinfo(ressave);

        return(listenfd);
}


char * sock_ntop(const struct sockaddr *sa, socklen_t salen){
    char    portstr[8];
    static char str[128];		/* Unix domain is largest */

    switch (sa->sa_family) {
    case AF_INET: {
        struct sockaddr_in	*sin = (struct sockaddr_in *) sa;

        if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) == NULL)
	    return(NULL);
	if (ntohs(sin->sin_port) != 0) {
	    snprintf(portstr, sizeof(portstr), ":%d", ntohs(sin->sin_port));
            strcat(str, portstr);
	}
	return(str);
    }
/* end sock_ntop */

#ifdef	IPV6
    case AF_INET6: {
	struct sockaddr_in6	*sin6 = (struct sockaddr_in6 *) sa;

	str[0] = '[';
	if (inet_ntop(AF_INET6, &sin6->sin6_addr, str + 1, sizeof(str) - 1) == NULL)
	    return(NULL);
	if (ntohs(sin6->sin6_port) != 0) {
	    snprintf(portstr, sizeof(portstr), "]:%d", ntohs(sin6->sin6_port));
	    strcat(str, portstr);
	    return(str);
        }
	return (str + 1);
    }
#endif
    default: {
	snprintf(str, sizeof(str), "sock_ntop: unknown AF_xxx: %d, len %d",
				 sa->sa_family, salen);
        return(str);
    }
    }
    return (NULL);
}


int readline_timeout(int ifd, char * buf, int max, int sec){
    struct timeval tv;
    fd_set readfds;
    int n=0, i=0;
    tv.tv_sec = sec;
    tv.tv_usec = 0;
    FD_ZERO(&readfds);
    FD_SET(ifd, &readfds);
    memset(buf,0,max);

    if(select(ifd+1, &readfds, NULL, NULL, &tv)==0){
        log_error("readwithtimeout(): time out!\n");
        return -2;
    }

    if(FD_ISSET(ifd,&readfds)){
        while(1){
	    if(i>max){
		log_debug("line input exceeds the buffer\n");
		return -1;
	    }
	    n=read(ifd,buf,1);
	    if(n==1){		
		if(((*buf)=='\n')){
		    return i+1;
		}
            i++;
            buf++;
	    }else if(n==0){
		log_error("connection is closed!1\ni=%d sockfd=%d\n",i,ifd);	
		close(ifd);
		return -1;
	    }else{
		log_error("readline_timeout()-read() return error:%s\n",strerror(errno));
		close(ifd);
		return -1;
	    }
	}
    }
    return 0;
}
