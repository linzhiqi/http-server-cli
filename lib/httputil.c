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
#define MAX_URI 500
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

enum processing_state{ init, start_line_parsed, content_length_got, body_reached, request_done, response_ready, response_sent };
enum request_type{ unsupported, get, put };
struct transaction_info{
  enum processing_state pro_state;
  enum request_type req_type;
  char * uri;
  char * doc_root;
  char * local_path;
  long body_size;
  long body_bytes_got;
  char * buf;
  int buf_offset;
  int bytes_in_buf;
  int sockfd;
  int fd;
  int file_existed;
  struct node * file_node;
  int file_lock_is_got;
  int resp_code;
};



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



int parse_req_start_line(struct transaction_info *info){
  char err_buf[501], * ptr;
  memset(err_buf,0,501);
  if(info->buf==NULL || info->uri==NULL){
    log_error("parse_req_start_line() buf or uri refers to NULL.\n");
    info->resp_code=500;
    return -1;
  }
  
  ptr=strstr(info->buf, "GET");
  if(ptr!=NULL && ptr==info->buf){
    info->pro_state=start_line_parsed;
    info->req_type=get;
    ptr=strstr(info->buf, "HTTP/1.");
    strncpy(info->uri, info->buf+4, (ptr-info->buf)-5);
    return 0;
  }
  ptr=strstr(info->buf, "PUT");
  if(ptr!=NULL && ptr==info->buf){
    info->pro_state=start_line_parsed;
    info->req_type=put;
    ptr=strstr(info->buf, "HTTP/1.");
    strncpy(info->uri, info->buf+4, (ptr-info->buf)-5);
    return 0;
  }
  info->resp_code=400;
  return -1;
}

long get_content_length(struct transaction_info *info){
  long len;
  char * ptr;
  if(info->buf==NULL){
    log_error("parse_req_start_line() buf refers to NULL.\n");
    info->resp_code=500;
    return -1;
  }
  if(info->req_type==get){
    info->body_size=0;
    info->pro_state=content_length_got;
    return 0;
  }

  if(ptr==NULL){
    return -1;
  }
  ptr+=15;
  len=strtol(ptr,&ptr,10);
  info->body_size=len;
  info->pro_state=content_length_got;
  return info->body_size;
}

int reach_body(struct transaction_info *info){
  char * ptr;
  int offset;
  if(info->buf==NULL){
    log_error("parse_req_start_line() buf refers to NULL.\n");
    info->resp_code=500;
    return -1;
  }
  ptr=strstr(info->buf,"\r\n\r\n");
  if(ptr==NULL){
    return -1;
  }
  if(info->req_type==get){
    info->pro_state=request_done;
  }else if(info->req_type==put){
    offset=ptr+4-info->buf;
    if(offset>info->bytes_in_buf+1){
      log_error("empty line and body is not in the same buffer. This implementation doesn't handle this situation.\n");
      info->resp_code=500;
      return -1;
    }
    info->buf_offset=offset;
    info->pro_state=body_reached;
  }
  return 0;
}

int get_fd_for_uri(struct transaction_info *info){
  char err_buf[500];
  memset(info->local_path,0,MAX_URI+1);
  strcpy(info->local_path,info->doc_root);
  strcat(info->local_path,info->uri);
  info->file_existed=file_exist(info->local_path);
  if((info->fd=creat(info->local_path,S_IRWXO|S_IRWXG|S_IRWXU))==-1){
    log_error("serve_put()-create() error:%s\n",strerror_r(errno,err_buf,500));
    info->resp_code=500;
    return -1;
  }
  return 0;
}

void get_file_wlock(struct transaction_info *info){
  init_file_list();
  info->file_node=getNode(info->local_path, fileLinkedList); 
  log_debug("getNode() for %s returned\n",info->local_path);   
  /*get node structure for this file*/
  if( info->file_node != NULL ){
    /*write lock the file*/
    pthread_rwlock_wrlock(info->file_node->mylock);
    log_debug("holds write-lock for %s\n",info->local_path);
  }else{
    info->file_node = appendNode(info->local_path, fileLinkedList);
    log_debug("appendNode() for %s returned\n",info->local_path);
    /*write lock the file*/
    pthread_rwlock_wrlock(info->file_node->mylock);
    log_debug("holds write-lock for %s\n",info->local_path);
  }
}

void get_file_rlock(struct transaction_info *info){
  init_file_list();
  info->file_node=getNode(info->local_path, fileLinkedList); 
  log_debug("getNode() for %s returned\n",info->local_path);   
  /*fetch node structure for this file*/
  if( info->file_node != NULL ){
    /*read lock the file*/
    pthread_rwlock_rdlock(info->file_node->mylock);
    log_debug("holds read-lock for %s\n",info->local_path);
  }else{
    info->file_node = appendNode(info->local_path, fileLinkedList);
    log_debug("appendNode() for %s returned\n",info->local_path);
    /*read lock the file*/
    pthread_rwlock_rdlock(info->file_node->mylock);
    log_debug("holds read-lock for %s\n",info->local_path);
  }
}

void release_file_lock(struct transaction_info *info){
  if(info->file_node!=NULL){
    /*release write_lock*/
    pthread_rwlock_unlock(info->file_node->mylock);
    info->file_node=NULL;
    log_debug("release lock for %s\n",info->local_path);
  }
}

int write_data_to_fd(struct transaction_info *info){
  int len_tobe_written=info->bytes_in_buf-info->buf_offset;
  if(writen(info->fd,info->buf+info->buf_offset,len_tobe_written)!=len_tobe_written){
    log_error("serve_put()-writen():data written to file is not completed\n");
    close(info->fd);
    info->resp_code=500;
    return -1;
  }
  info->body_bytes_got+=len_tobe_written;
  if(info->body_bytes_got>=info->body_size){
    if(!info->file_existed){
      info->resp_code=201;
    }else{
      info->resp_code=200;
    }
    info->pro_state=request_done;
  }
  return 0;
}

char * get_resp_reason(int code){
  switch (code){
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 500:
      return "Internal Error";
    case 404:
      return "Not Found";
    case 400:
      return "Bad Request";
    default:
      return "Response code not supporteds";
  }
}

void handle_put_req(struct transaction_info *info){
  sprintf(info->buf,"HTTP/1.1 %d %s\r\nIam: linzhiqi\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n",info->resp_code,get_resp_reason(info->resp_code),0);
  info->pro_state=response_ready;
}

int is_index_uri(char * uri){
  return strcmp(uri,"/")==0||strcmp(uri,"/index")==0||strcmp(uri,"/index.html")==0;
}

void handle_index(struct transaction_info *info){
  char *body, dir_info[4000];
  if(list_file_info(info->doc_root, dir_info, 4000)==0){
    info->resp_code=200;
    body=dir_info;
    info->body_size=strlen(dir_info);
  }else{
    info->resp_code=500;
    body="";
    info->body_size=0;
  }
  
  sprintf(info->buf,"HTTP/1.1 %d %s\r\nIam: linzhiqi\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s",info->resp_code,get_resp_reason(info->resp_code),info->body_size,body);
  log_debug("handle index():%s\n",info->buf); 
  info->pro_state=response_ready;
}

void handle_get_req(struct transaction_info *info){
  char err_buf[500];
  memset(info->local_path,0,MAX_URI+1);
  strcpy(info->local_path,info->doc_root);
    log_debug("handle_get_req()\n");
  strcat(info->local_path,info->uri);
  if((info->file_existed=file_exist(info->local_path))==0){
    info->resp_code=404;
    info->body_size=0;
  }else{
    /*if resource exists*/
    get_file_rlock(info);
    /*open file, copy and write to client*/
    if((info->fd=open(info->local_path,O_RDWR))==-1)
    {
      log_error("handle_get_req()-open() error:%s\n",strerror_r(errno,err_buf,500));
      info->resp_code=500;
      info->body_size=0;
    }else{
      info->resp_code=200;
      info->body_size=file_size(info->local_path);
    }
  }
  sprintf(info->buf,"HTTP/1.1 %d %s\r\nIam: linzhiqi\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n",info->resp_code,get_resp_reason(info->resp_code),info->body_size);
  if(writenwithtimeout(info->sockfd,info->buf,strlen(info->buf),10)!=strlen(info->buf))
  {
    log_error("info->buf is not sent completely!\n");
    close(info->sockfd);
    exit(-1);
  }
  if(info->resp_code==200){
    sendfile(info->fd,info->body_size, info->sockfd);
  }
  release_file_lock(info);
}

void handle_unsupported_req(struct transaction_info *info){

}

char *print_transaction_state(struct transaction_info *info){
  char * ptr=(char *)malloc(300);
  memset(ptr+299,0,1);
  snprintf(ptr,299,"transaction info:\npro_state=%d\nuri=%s\ndoc_root=%s\nbody_size=%ld\nbody_bytes_got=%ld\nbuf_offset=%d\nbytes_in_buf=%d\nfile_existed=%d\nfile_lock_is_got=%d\nresp_code=%d\n", info->pro_state,info->uri, info->doc_root, info->body_size, info->body_bytes_got, info->buf_offset, info->bytes_in_buf, info->file_existed, info->file_lock_is_got, info->resp_code);
  return ptr;
}

void serve_http_request(int sockfd, char * doc_root){
  int bytes_read=0;
  char httpMsg[MAXMSGBUF+1], uri[MAX_URI+1], root[MAX_URI+1], file_path[MAX_URI+1], *ptr;
  struct transaction_info *info;

  memset(httpMsg,0,MAXMSGBUF+1);
  memset(uri,0,MAX_URI+1);
  memset(root,0,MAX_URI+1);
  memset(file_path,0,MAX_URI+1);
  strncpy(root,doc_root,MAX_URI);
  info=(struct transaction_info *)malloc(sizeof(struct transaction_info));
  info->pro_state=init;
  info->sockfd=sockfd;
  info->buf=httpMsg;
  info->uri=uri;
  info->doc_root=root;
  info->local_path=file_path;
  info->file_node=NULL;
  info->fd=-1;
  info->file_lock_is_got=0;
  
  while(info->pro_state!=request_done){
    log_debug("info->pro_state!=request_done\n");
    if((bytes_read = readwithtimeout(sockfd, info->buf, MAXMSGBUF,10))<=0){
      ptr=print_transaction_state(info);
      log_error("serve_http_request()-readwithtimeout() error. %s\n", ptr);
      free(ptr);
      exit(-1);
    }
    info->buf_offset=0;
    info->bytes_in_buf=bytes_read;
    if(info->pro_state==init && parse_req_start_line(info)==-1){
      continue;
    }
    ptr=print_transaction_state(info);
    log_debug("after parse_req_start_line(info):%s\n",ptr);
    free(ptr);
    if(info->pro_state==start_line_parsed && get_content_length(info)==-1){
      continue;
    }
    ptr=print_transaction_state(info);
    log_debug("after get_content_length(info):%s\n",ptr);
    free(ptr);
    if(info->pro_state==content_length_got && reach_body(info)==-1){
      continue;
    }
    ptr=print_transaction_state(info);
    log_debug("after reach_body(info):%s\n",ptr);
    free(ptr);
    if(info->req_type==put && info->pro_state==body_reached){
      if(info->fd==-1){
        get_fd_for_uri(info);
      }
      if(!info->file_lock_is_got){
        get_file_wlock(info);
      }
      write_data_to_fd(info);
    }
  }
  if(info->file_lock_is_got){
    release_file_lock(info);
  }

  if(info->pro_state==request_done){
    memset(info->buf,0,MAXMSGBUF+1);
    if(info->req_type==put){
      handle_put_req(info);
    }else if(info->req_type==get && is_index_uri(info->uri)){
      handle_index(info);
    }else if(info->req_type==get && !is_index_uri(info->uri)){
      handle_get_req(info);
    }else{
      handle_unsupported_req(info);
    }
    if(info->pro_state==response_ready){
      if(writenwithtimeout(info->sockfd,info->buf,strlen(info->buf),10)!=strlen(info->buf))
      {
        log_error("info->buf is not sent completely!\n");
        close(info->sockfd);
        exit(-1);
      }
    }
  }
}












































