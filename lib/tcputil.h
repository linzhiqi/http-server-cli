#ifndef NWPROG_LINZHIQI_TCPUTIL_H
#define NWPROG_LINZHIQI_TCPUTIL_H

#include <sys/types.h>
#include <sys/socket.h>

int tcp_connect(const char *host, const char *serv);
int tcp_listen(const char *host, const char *serv, socklen_t *addrlenp);
char * sock_ntop(const struct sockaddr *sa, socklen_t salen);
ssize_t writen(int fd, const char *buf, size_t len);
ssize_t writewithtimeout(int sockfd, const void *buf, size_t count, int sec);
ssize_t writenwithtimeout(int fd, const char *buf, size_t len, int sec);
ssize_t readwithtimeout(int sockfd, void *buf, size_t count, int sec);
int readline_timeout(int ifd, char * buf, int max, int sec);

#endif
