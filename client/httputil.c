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
#include <math.h>
#include <signal.h>
#include <errno.h>

#define MAXFILENAME 100
#define MAXHOSTNAME 200
#define MAX_URL_LEN 500
#define MAX_PORT_LEN 5
#define MAX_LOCATION_LEN 200
#define MAXMSGBUF 5000
#define DEFAULT_FILE_NAME "downloaded_file"
#ifndef max
#define max(a,b)            (((a) < (b)) ? (b) : (a))
#endif

ssize_t writen(int fd, const char *buf, size_t len);
char *getFileFromRes(char * file_name, const char * res_location);
int getofd(const char * res_location, const char *local_path, int store_data);
int parserespcode(char ** pptr);
long parselength(char ** pptr);
int parsebodystart(char ** pptr);
ssize_t readwithtimeout(int sockfd, void *buf, size_t count, int sec);
ssize_t writewithtimeout(int sockfd, const void *buf, size_t count, int sec);
ssize_t writenwithtimeout(int fd, const char *buf, size_t len, int sec);

/* url:=[protocol://]+<hostname>+[:port number]+[resource location] */
void parse_url(const char *url, char *port, char *host, char *location){
    char buf[MAX_URL_LEN];
    char * ptr, *ptr2, * ptr3;
    strcpy(buf, url);
    /*first remove the protocol part if there's any*/
    ptr=strstr(buf,"://");
    if(ptr==NULL){
        ptr=buf;
    }else{
        ptr+=3;
    }
    
    /*then, find and get resource location.if not specified, use '/' as default*/
    ptr2=strstr(ptr,"/");
    if(ptr2 == NULL){
        *location='/';
    }else{
        strcpy(location,ptr2);
        *ptr2='\0';
    }
    /*then find the port by ':'. if not specified, use '80' as default*/
    ptr3=strstr(ptr,":");
    if(ptr3 == NULL){
        strcpy(port,"80");
    }else{
        *ptr3='\0';
        ptr3++;
        strcpy(port, ptr3);
    }
    strcpy(host, ptr);
    return;
}


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
 *create and send http GET method to fetch body data
 *fetched data is stored into local file while store_data==1 or just be printed to console if store_data==0
 */
int fetch_body(int sockfd, char * res_location, const char * hostName, const char *local_path, int store_data)
{
    long int n=0,total=0,len=-1,code=0;
    char httpMsg[MAXMSGBUF];
    char *ptr;
    int fd,bodystartfl=0,ofdgotfl=0;
    size_t tobewritten;

    sprintf(httpMsg,"GET %s HTTP/1.1\r\nHost:%s\r\nIam: linzhiqi\r\n\r\n",res_location,hostName);
    if(writenwithtimeout(sockfd,httpMsg,strlen(httpMsg),10)!=strlen(httpMsg))
    {
        printf("GET message is not sent completely\n");
        close(sockfd);
        return -1;
    }

    memset(httpMsg,0,MAXMSGBUF);
    while((n = readwithtimeout(sockfd, httpMsg, MAXMSGBUF-1,10))>0){
        ptr=httpMsg;
        if(code==0){
            code=parserespcode(&ptr);
        }
        if(len==-1){
            len=parselength(&ptr);
        }
        if(bodystartfl==0){
            bodystartfl=parsebodystart(&ptr);
        }
        if(code==200 && len>0 && bodystartfl){
            if(ofdgotfl==0){
	        if((fd=getofd(res_location,local_path,store_data))==-1) return -1;
                ofdgotfl=1;
            }
            tobewritten=strlen(ptr);
            if(writen(fd,ptr,tobewritten)!=tobewritten)
            {
                printf("Received body content is not written completely\n");
                close(fd);
                return -1;
            }
            total+=tobewritten;
            if(total>=len)
	    {
	        close(fd);
                return 0;
	    }
        }
        memset(httpMsg,0,MAXMSGBUF);
    }

    if(n==-1)
    {
	perror("read error");
	return -1;
    }else if (n==0){
	printf("connection is closed by server, however body is not received completely.\n");
	return -1;
    }else if (n==-2){
        printf("fetch_body(): read time out!\n");
        return -1;
    }
    return 0;
}

int ignoresig(int signo){
    struct sigaction	act, oact;
    act.sa_handler=SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (sigaction(signo, &act, &oact) < 0){
        perror("ignoresig");
        return -1;
    }
    return 0;
}

int sendfile(int fd, int file_size, int sockfd){
    fd_set rset;
    int maxfdp1, stdineof, code, n, total;
    char buf[200], *ptr;
    
    stdineof = 0;
    FD_ZERO(&rset);
    ignoresig(SIGPIPE);
    for ( ; ; ) {
        if (stdineof == 0)
            FD_SET(fd, &rset);
        FD_SET(sockfd, &rset);
        maxfdp1 = max(fd, sockfd) + 1;
        
        select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(sockfd, &rset)) {	/* socket is readable */
            if ( (n = read(sockfd, buf, MAXMSGBUF)) == 0) {
		log_error("sendfile()-read() server terminated prematurely; error:%s\n",strerr(errno));
                return(-1);
                
	    }else if(n<0){
                log_error("sendfile()-read() error:%s\n",strerr(errno));
                return(-1);
            }else{
		log_error("sendfile() received response before sending complete; response:%s\n",buf);
                return -1;
                /*ptr = buf;
                code=parserespcode(&ptr);
                if((code/100)==2)
                {
		    printf("Put respond OK.\n");
		    return 0;
                }else{
		    printf("Put respond not OK.\n");
		    return -1;
                }*/
            }
	}

	if (FD_ISSET(fd, &rset)) {  /* input is readable */
            if ( (n = read(fd, buf, MAXMSGBUF)) == 0) {
                stdineof = 1;
		FD_CLR(fd, &rset);
		continue;
            }
            tmp = total+n;
            if(tmp>file_size) n = file_size-total;
            if(writenwithtimeout(sockfd,buf,n,10)!=n)
            {
                close(fd);
                return(-1);
            }
            total+=n;
            if(total>=file_size) return 0;
        }
    }
}


/*
 *create PUT method and send it with body data read from local file
 */
int upload_file(int sockfd, const char * res_location, const char * hostName, const char *local_path)
{
    long int len=0;
    char httpMsg[MAXMSGBUF];
    int fd,ret,code;
    
    struct stat fileinfo;

    /*get the file size of the object*/
    if(stat(local_path, &fileinfo)==-1)
    {
        perror("file info error");
        return -1;
    }
    len=(long int)fileinfo.st_size;

    sprintf(httpMsg,"PUT %s HTTP/1.1\r\nHost:%s\r\nIam: linzhiqi\r\nContent-Length: %ld\r\n\r\n",res_location,hostName,len);

    if(writenwithtimeout(sockfd,httpMsg,strlen(httpMsg),10)!=strlen(httpMsg))
    {
        printf("PUT headers are not sent completely!\n");
        close(sockfd);
        return -1;
    }
	
    if((fd=open(local_path,O_RDWR))==-1)
    {
        perror("open local file");
        return -1;
    }

    ret=sendfile(fd, len, sockfd);
    if(ret==0){
        readline_timeout(sockfd,httpMsg,10);
        code=parserespcode(&httpMsg);
        close(sockfd);
        if((code/100)==2)
        {
            log_debug("Put respond OK.\n");
	    return 0;
        }else{
	    log_error("Put respond not OK.\n");
	    return -1;
        }
    }else{
        return -1;
    }
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

/*
 *get the part after the last '/' as file name
 *if nothing after the last '/', return NULL
 */
char *getFileFromRes(char * file_name, const char * res_location){
    char * ptr;
    if(res_location==NULL) return NULL;
    
    ptr=strrchr(res_location, '/');
    if(*(ptr++)=='\0') return NULL;
    strcpy(file_name, ptr);
    return file_name;
}

/*
 * return the file descriptor according to the parameters
 * the logic is if store_data==0, it returns 1 which is the standart output.
 * if store_data==1, it create a local file specified by argument local_path, and returns the file descriptor of it.
 * if local_path is empty or NULL, it use argument res_location, the resource part of a url, to determine the file name.
 */
int getofd(const char * res_location, const char *local_path, int store_data){
    const char * filename;
    int fd;
    if(store_data==1){
	/*obtain the file name from resource location, if not specified*/
        if(local_path==NULL || strcmp(local_path,"")==0){
            char buf[MAXFILENAME];
            if(getFileFromRes(buf, res_location)==NULL) filename=DEFAULT_FILE_NAME;
            else filename=buf;
        }else{
            filename=local_path;
        }

        if((fd=creat(filename,S_IRWXO|S_IRWXG|S_IRWXU))==-1)
	{
	    perror("file create error");
	    return -1;
        }
    }else{
        fd=1;
    }
    return fd;
}

/*
 * return the response code if it's found in the string *pptr, and move its starting place to the first char after the code. 
 * if code string is not found in the string, return 0 and leave the string un-changed.
 */
int parserespcode(char ** pptr){
    int code=0;
    char * ptr;
    ptr=strstr(*pptr,"HTTP/1.1");
    if(ptr==NULL){
        return code;
    }
    ptr+=8;
    /*get the response code*/
    code=(int)strtol(ptr,&ptr,10);
    *pptr=ptr;
    return code;
}

/*
 * return the value of Content_Length if it's found in the string *pptr, and move its starting place to the first char after the length value. 
 * if Content-Length header string is not found in the string, return -1 and leave the string un-changed.
 */
long parselength(char ** pptr){
    long len=-1;
    char * ptr;
    ptr=strstr(*pptr,"Content-Length");
    if(ptr==NULL){
        return len;
    }
    ptr+=15;
    len=strtol(ptr,&ptr,10);
    *pptr=ptr;
    return len;
}

/*
 * return 1 if the starting place of http body is found in the string *pptr, and move its starting place to the first char after the blank line. 
 * if the blank line is not found in the string, return 0 and leave the string un-changed.
 */
int parsebodystart(char ** pptr){
    char * ptr;
    ptr=strstr(*pptr,"\r\n\r\n");
    if(ptr==NULL){
        return 0;
    }else{
        ptr+=4;
        *pptr=ptr;
        return 1;
    }
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
