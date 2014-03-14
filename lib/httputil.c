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
#include <pthread.h>
#include "logutil.h"
#include "util.h"
#include "tcputil.h"
#include "linkedlist.h"


#define MAXFILENAME 100
#define MAXHOSTNAME 200
#define MAX_URL_LEN 500
#define MAX_PORT_LEN 5
#define MAX_LOCATION_LEN 200
#define MAXMSGBUF 5000
#define MAXHEADER 1000
#define DEFAULT_FILE_NAME "downloaded_file"
#ifndef max
#define max(a,b)            (((a) < (b)) ? (b) : (a))
#endif

ssize_t writen(int fd, const char *buf, size_t len);
char *getFileFromRes(char * file_name, const char * res_location);
int getofd(const char * res_location, const char *local_path, int store_data);
int parserespcode(char * buf);
long parselength(char ** pptr);
int parsebodystart(char ** pptr);
ssize_t readwithtimeout(int sockfd, void *buf, size_t count, int sec);
ssize_t writewithtimeout(int sockfd, const void *buf, size_t count, int sec);
ssize_t writenwithtimeout(int fd, const char *buf, size_t len, int sec);

extern struct node * fileLinkedList;

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
        log_error("GET message is not sent completely\n");
        close(sockfd);
        return -1;
    }

    memset(httpMsg,0,MAXMSGBUF);
    while((n = readwithtimeout(sockfd, httpMsg, MAXMSGBUF-1,10))>0){
        ptr=httpMsg;
        if(code==0){
            code=parserespcode(ptr);
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
                log_error("Received body content is not written completely\n");
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
	log_error("fetch_body() read error:%s", strerror(errno));
	return -1;
    }else if (n==0){
	log_error("fetch_body() connection prematurely closed:%s", strerror(errno));
	return -1;
    }else if (n==-2){
        log_error("fetch_body(): read time out!\n");
        return -1;
    }
    return 0;
}


int sendfile(int fd, int file_size, int sockfd){
    fd_set rset;
    int maxfdp1, stdineof, n, total=0, tmp;
    char buf[MAXMSGBUF+1],err_buf[501];
    
    memset(err_buf,0,501);
    stdineof = 0;
    FD_ZERO(&rset);
    ignoresig(SIGPIPE);
    for ( ; ; ) {
        memset(buf,0,MAXMSGBUF+1);
        if (stdineof == 0)
            FD_SET(fd, &rset);
        FD_SET(sockfd, &rset);
        maxfdp1 = max(fd, sockfd) + 1;
        
        select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(sockfd, &rset)) {	/* socket is readable */
            if ( (n = read(sockfd, buf, MAXMSGBUF)) == 0) {
		log_error("sendfile()-read() server terminated prematurely; error:%s\n",strerror_r(errno,err_buf,500));
                return(-1);
                
	    }else if(n<0){
                log_error("sendfile()-read() error:%s\n",strerror_r(errno,err_buf,500));
                return(-1);
            }else{
		log_error("sendfile() received response before sending complete; response:%s\n",buf);
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
            if(total>=file_size){
                close(fd);
                return 0;
            }
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
        log_error("upload_file()-stat():%s",strerror(errno));
        return -1;
    }
    len=(long int)fileinfo.st_size;

    sprintf(httpMsg,"PUT %s HTTP/1.1\r\nHost:%s\r\nIam: linzhiqi\r\nContent-Length: %ld\r\n\r\n",res_location,hostName,len);

    if(writenwithtimeout(sockfd,httpMsg,strlen(httpMsg),10)!=strlen(httpMsg))
    {
        log_error("PUT headers are not sent completely!\n");
        close(sockfd);
        return -1;
    }
	
    if((fd=open(local_path,O_RDWR))==-1)
    {
        log_error("upload_file()-open(%s):%s",local_path,strerror(errno));
        return -1;
    }

    ret=sendfile(fd, len, sockfd);
    if(ret==0){
        readline_timeout(sockfd,httpMsg,MAXMSGBUF,10);
        code=parserespcode(httpMsg);
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
            log_error("getofd():%s",strerror(errno));
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
int parserespcode(char * buf){
    int code=0;
    char * ptr;
    ptr=strstr(buf,"HTTP/1.1");
    if(ptr==NULL){
        return code;
    }
    ptr+=8;
    /*get the response code*/
    code=(int)strtol(ptr,&ptr,10);
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




/*
 * input the string buffer to be parsed, and return the strings of method type and URI
 */
int parse_req_line(const char * req_line, char * method_ptr, char * uri_ptr){
    char err_buf[501], * ptr;
    memset(err_buf,0,501);
    ptr=strstr(req_line, "GET");
    if(ptr!=NULL && ptr==req_line){
        strcpy(method_ptr, "GET");
        ptr=strstr(req_line, "HTTP/1.");
        strncpy(uri_ptr, req_line+4, (ptr-req_line)-5);
        return 0;
    }
    ptr=strstr(req_line, "PUT");
    if(ptr!=NULL && ptr==req_line){
        strcpy(method_ptr, "PUT");
        ptr=strstr(req_line, "HTTP/1.");
        strncpy(uri_ptr, req_line+4, (ptr-req_line)-5);
        return 0;
    }
    log_debug("Request line not supported:%s\n", req_line);
    return -1;
}

int file_exist(const char *filename){
    struct stat   fileinfo;   
    return (stat(filename, &fileinfo) == 0);
}

int file_size(const char *filename){
    int size=0;
    char err_buf[501];
    struct stat fileinfo;
    memset(err_buf,0,501);
    if(stat(filename, &fileinfo)==-1){
        log_error("file_size()-stat() file '%s' return error:%s\n",filename,strerror_r(errno,err_buf,500));
        size=-1;
    }
    size=(int)fileinfo.st_size;
    return size;
}

void serve_index(int connfd, const char * root_path){
    char httpmsg[MAXMSGBUF+1], fileinfo[4000];
    int code;
    char *reason;

    if(list_file_info(root_path, fileinfo, 4000)==0){
        code=200;
        reason="OK";
    }else{
        code=404 ;
        reason="Not Found";
    }
    sprintf(httpmsg,"HTTP/1.1 %d %s\r\nIam: linzhiqi\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s",code,reason,(int)strlen(fileinfo),fileinfo);
    if(writenwithtimeout(connfd,httpmsg,strlen(httpmsg),10)!=strlen(httpmsg))
    {
        log_error("serve_index() response are not sent completely!\n");
        close(connfd);
        exit(-1);
    }
    log_debug("GET response:%d,%s\n",code,reason);
}

void serve_get(int connfd, const char * root_path, const char * uri_ptr){
    char path[MAX_LOCATION_LEN];
    struct node * file_node;
    int fd, code=200;
    char *reason="OK";
    int filesize=0;
    char headers[MAXHEADER+1], err_buf[501];
    memset(err_buf,0,501);
    memset(headers,0,MAXHEADER+1);
    memset(path,0,MAX_LOCATION_LEN);
    log_debug("root_path: %s\n",root_path);
    /*obtain file path from uri_ptr*/
    if(strcmp(uri_ptr,"/")==0||strcmp(uri_ptr,"/index")==0||strcmp(uri_ptr,"/index.html")==0){
        serve_index(connfd,root_path);
	return;
    }else{
        strcpy(path,root_path);
        strcat(path,uri_ptr);
    }

    log_debug("path: %s, exits?%d, size:%d\n", path, file_exist(path),file_size(path));
    /*read up the reqeust message*/
    readwithtimeout(connfd, headers, MAXHEADER,10);
    init_file_list();
    file_node=getNode(path, fileLinkedList);
    log_debug("getNode() for %s returned\n",path);
    /*add node for this file if not exists yet*/
    if( file_node != NULL ){
        /*read lock the file*/
        pthread_rwlock_rdlock(file_node->mylock);
        log_debug("holds read-lock for %s\n",path);
    }else if(file_exist(path)){
        file_node = appendNode(path, fileLinkedList);
        log_debug("appendNode for %s returned\n",path);
        /*read lock the file*/
        pthread_rwlock_rdlock(file_node->mylock);
        log_debug("holds read-lock for %s\n",path);
    }else{
        code=404;
        reason="Not Found";
    }
    
    if(code!=404) filesize=file_size(path);

    /*create response*/
    sprintf(headers,"HTTP/1.1 %d %s\r\nIam: linzhiqi\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n",code,reason,filesize);

    /*write response*/
    if(writenwithtimeout(connfd,headers,strlen(headers),10)!=strlen(headers))
    {
        log_error("serve_get() headers are not sent completely!\n");
        close(connfd);
        exit(-1);
    }
    if(code==404){
        if(close(connfd)!=0) log_error("serve_get()-close(socket) error:%s\n",strerror_r(errno,err_buf,500));
        return;
    }
    /*open file, copy and write to client*/
    if((fd=open(path,O_RDWR))==-1)
    {
        log_error("serve_get()-open() error:%s\n",strerror_r(errno,err_buf,500));
        exit(-1);;
    }
    sendfile(fd,filesize, connfd);
    /*release the file*/
    pthread_rwlock_unlock(file_node->mylock);
    log_debug("release lock for %s\n",path);
    log_debug("GET response:%d,%s\n",code,reason);
    /*close socket*/
    if(close(connfd)!=0) log_error("serve_get()-close(socket) error:%s\n",strerror_r(errno,err_buf,500));
}

void serve_put(int sockfd, const char * root_path, const char * uri_ptr){
    char path[MAX_LOCATION_LEN],err_buf[501],*ptr;
    struct node * file_node;
    int fd, code=0;
    char *reason;
    int bodystartfl=0,file_create_flg=0,file_existed=0;
    char headers[MAXHEADER];
    char httpMsg[MAXMSGBUF+1];
    long int n=0,total=0,len=-1,len_tobe_written=0;

    memset(err_buf,0,501);
    memset(path,0,MAX_LOCATION_LEN);
    memset(httpMsg,0,MAXMSGBUF+1);
    /*obtain file path from uri_ptr*/
    if(strcmp(uri_ptr,"/")==0){
        code=400;
        reason="Bad request";
    }else{
        strcpy(path,root_path);
        strcat(path,uri_ptr);
    }
    log_debug("root_path:%s,uri_ptr:%s,path:%s, exits?%d\n", root_path, uri_ptr, path, file_exist(path));
    init_file_list();
    file_node=getNode(path, fileLinkedList); 
    log_debug("getNode() for %s returned\n",path);   
    /*ftech write_lock for this file*/
    if( file_node != NULL ){
        /*write lock the file*/
        pthread_rwlock_wrlock(file_node->mylock);
        log_debug("holds write-lock for %s\n",path);
    }else{
        file_node = appendNode(path, fileLinkedList);
        log_debug("appendNode() for %s returned\n",path);
        /*write lock the file*/
        pthread_rwlock_wrlock(file_node->mylock);
        log_debug("holds write-lock for %s\n",path);
    }

    file_existed=file_exist(path);
    /*read body and create or override the file*/
    while((n = readwithtimeout(sockfd, httpMsg, MAXMSGBUF,10))>0){
        ptr=httpMsg;
        /*read and obtain content_length*/
        if(len==-1){
            len=parselength(&ptr);
        }
        /*read to the first byte of body*/
        if(bodystartfl==0){
            bodystartfl=parsebodystart(&ptr);
        }
        len_tobe_written=strlen(ptr);
        
	if(!file_create_flg)
        {
            if((fd=creat(path,S_IRWXO|S_IRWXG|S_IRWXU))==-1)
	    {
	        log_error("serve_put()-create() error:%s\n",strerror_r(errno,err_buf,500));
	        code=500;
                reason="Internal Error";
                break;
            }
            file_create_flg=1;
        }
        if(writen(fd,ptr,len_tobe_written)!=len_tobe_written)
        {
            log_error("serve_put()-writen():data written to file is not completed\n");
            close(fd);
            code=500;
            reason="Internal Error";
            break;
        }
        total+=len_tobe_written;
        if(total>=len)
	{
            if(!file_existed){
                code=201;
                reason="Created";
            }else{
                code=200;
                reason="OK";
            }
            break;
	}
        
        memset(httpMsg,0,MAXMSGBUF+1);
    }
   
    /*release write_lock*/
    pthread_rwlock_unlock(file_node->mylock);
    log_debug("release lock for %s\n",path);
    /*create response*/
    sprintf(headers,"HTTP/1.1 %d %s\r\nIam: linzhiqi\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n",code,reason,0);
    /*write response*/
    if(writenwithtimeout(sockfd,headers,strlen(headers),10)!=strlen(headers))
    {
        log_error("serve_get() headers are not sent completely!\n");
        close(sockfd);
        exit(-1);
    }
    log_debug("PUT response:%d,%s\n",code,reason);;
}

