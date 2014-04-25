#include <sys/socket.h>
#include <strings.h>
#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>
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
#include "dnsutil.h"
#include "httputil.h"


#define MAXFILENAME 100
#define MAXHOSTNAME 200
#define MAX_URL_LEN 500
#define MAX_URI 500
#define MAX_PORT_LEN 5
#define MAX_LOCATION_LEN 200
#define MAXMSGBUF 5000
#define MAXHEADER 1000
#define MAX_DNS_MSG 512
#define DEFAULT_FILE_NAME "downloaded_file"
#ifndef max
#define max(a,b)            (((a) < (b)) ? (b) : (a))
#endif


char *getFileFromRes(char * file_name, const char * res_location);
int getofd(const char * res_location, const char *local_path, int store_data);
int parserespcode(char * buf);
long parselength(char ** pptr);
int parsebodystart(char ** pptr);

extern struct node * fileLinkedList;
char * dns_server;




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
        *(++location)='\0';
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
 * send POSR request and receive response
 */
void post_transaction(int sockfd, const char * uri, const char * host, const char * body){
  int n=0;
  char httpMsg[MAXMSGBUF];
  sprintf(httpMsg,"POST %s HTTP/1.1\r\nHost:%s\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nIam: linzhiqi\r\n\r\n%s",uri,host,(int)strlen(body),body);
  if(writenwithtimeout(sockfd,httpMsg,strlen(httpMsg),10)!=strlen(httpMsg))
  {
    log_error("POST message is not sent completely\n");
    close(sockfd);
    return;
  }
  memset(httpMsg,0,MAXMSGBUF);
  if((n = readwithtimeout(sockfd, httpMsg, MAXMSGBUF-1,25))>0){
    close(sockfd);
    log_debug("POST respond:\n%s\n",httpMsg);
  }else if(n==-2){
    log_error("post_transaction(): read time out!\n");
  }else if(n==0){
    log_error("post_transaction(): connection prematurely closed:%s", strerror(errno));
  }else{
    log_error("post_transaction():%s", strerror(errno));
  }
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
        }else{
            log_debug("response code = %d\n",code);
            return -1;
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
  ptr=strstr(info->buf, "POST");
  if(ptr!=NULL && ptr==info->buf){
    info->pro_state=start_line_parsed;
    info->req_type=post;
    ptr=strstr(info->buf, "HTTP/1.");
    strncpy(info->uri, info->buf+5, (ptr-info->buf)-5);
    return 0;
  }
  info->resp_code=400;
  info->pro_state=request_done;
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

  ptr=strstr(info->buf,"Content-Length");

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
  }else if(info->req_type==post){
    offset=ptr+4-info->buf;
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
  info->file_lock_is_got=1;
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
  info->file_lock_is_got=1;
}

void release_file_lock(struct transaction_info *info){
  if(info->file_node!=NULL){
    /*release write_lock*/
    pthread_rwlock_unlock(info->file_node->mylock);
    info->file_node=NULL;
    log_debug("release lock for %s\n",info->local_path);
    info->file_lock_is_got=0;
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
    case 501:
      return "Not Implemented";
    case 403:
      return "Forbidden";
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
  strcat(info->local_path,info->uri);
  log_debug("handle_get_req() file=%s exists=%d\n",info->local_path,file_exist(info->local_path));
  if((info->file_existed=file_exist(info->local_path))==0){
    log_debug("handle_get_req() file not exits.");
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
  log_debug("handle_get_req() buf:%s\n",info->buf);
  if(writenwithtimeout(info->sockfd,info->buf,strlen(info->buf),10)!=strlen(info->buf))
  {
    log_error("info->buf is not sent completely!\n");
    close(info->sockfd);
    exit(-1);
  }
  if(info->resp_code==200){
    sendfile(info->fd,info->body_size, info->sockfd);
  }
  if(info->file_lock_is_got==1){
    release_file_lock(info);
  }
}

void handle_unsupported_req(struct transaction_info *info){
  sprintf(info->buf,"HTTP/1.1 %d %s\r\nIam: linzhiqi\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n",info->resp_code,get_resp_reason(info->resp_code),0);
  info->pro_state=response_ready;
}

char *print_transaction_state(struct transaction_info *info){
  char * ptr=(char *)malloc(300);
  memset(ptr+299,0,1);
  snprintf(ptr,299,"transaction info:\nreq_type=%d\npro_state=%d\nuri=%s\ndoc_root=%s\nbody_size=%ld\nbody_bytes_got=%ld\nbuf_offset=%d\nbytes_in_buf=%d\nfile_existed=%d\nfile_lock_is_got=%d\nresp_code=%d\n", info->req_type,info->pro_state,info->uri, info->doc_root, info->body_size, info->body_bytes_got, info->buf_offset, info->bytes_in_buf, info->file_existed, info->file_lock_is_got, info->resp_code);
  return ptr;
}

void log_transaction_state(struct transaction_info *info, char * msg){
  char * ptr;
  ptr=print_transaction_state(info);
  log_debug("%s: %s\n", msg, ptr);
  free(ptr);
}

int is_dns_query(char * uri){
  if(strncmp(uri,"/dns-query",9)==0){
    return 1;
  }else{
    return 0;
  }
}

int parse_dns_query_in_post(const char * content, char * name, char * type){
  char * ptr1, * ptr2;
  int len=0;
  ptr1=strstr(content,"Name=");
  ptr2=strstr(content,"&Type=");
  if(ptr1==NULL || ptr2==NULL){
    return -1;
  }
  len=ptr2-ptr1-5;
  strncpy(name,ptr1+5,len);
  strncpy(type,ptr2+6,4);
  return 0;
}

void create_http_resp_dns(struct transaction_info *info, enum dns_rcode rcode, char * rdata_str){
  if(rcode==no_error){
    info->resp_code=200;
  }else if(rcode==format_error){
    info->resp_code=400;
  }else if(rcode==server_failure){
    info->resp_code=500;
  }else if(rcode==name_error){
    info->resp_code=404;
  }else if(rcode==not_imp){
    info->resp_code=501;
  }else if(rcode==refused){
    info->resp_code=403;
  }

  if(info->resp_code==200){
    sprintf(info->buf,"HTTP/1.1 %d %s\r\nIam: linzhiqi\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s",info->resp_code,get_resp_reason(info->resp_code),(int)strlen(rdata_str),rdata_str);
  }
  
  info->pro_state=request_done;
}

void create_failed_http_rsp(struct transaction_info *info, const char * code){
  sprintf(info->buf,"HTTP/1.1 %d %s\r\nIam: linzhiqi\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n",info->resp_code,get_resp_reason(info->resp_code),0);
  info->pro_state=response_ready;
}

void handle_dns_query(struct transaction_info *info, const char * name, const char * type, char * dns_result){
  int lost=0, udp_sockfd=-1, msg_size=0, num;
  uint8_t * dns_query_buf, dns_resp_buf[MAX_DNS_MSG];
  struct addrinfo * ressave, * res;
  char * rdata_str, *dns_server2;
  enum dns_rcode rcode;

  dns_query_buf=create_dns_query(name, type, &msg_size);
  /*create udp socket*/
  if(dns_server!=NULL){
    dns_server2=dns_server;
  }else{
    dns_server2="8.8.8.8";
  }
  if((udp_sockfd=create_udp_socket(dns_server2,"53", &res, &ressave))==-1){
    freeaddrinfo(ressave);
    free(dns_query_buf);
    log_error("create_udp_socket() failed");
    info->resp_code=500;
    info->pro_state=request_done;
    return;
  }
  /*setopt timeout*/
  setReadTimeout(udp_sockfd);
  
  do{
    if (sendto(udp_sockfd, dns_query_buf, msg_size, 0, res->ai_addr, res->ai_addrlen) < 0) {
      freeaddrinfo(ressave);
      free(dns_query_buf);
      log_error("sendto() failed error:%s.\n",strerror(errno));
      info->resp_code=500;
      info->pro_state=request_done;
      return;
    }

    if ((num=recvfrom(udp_sockfd, dns_resp_buf, MAX_DNS_MSG, 0, NULL, NULL)) < 0) {
      if (errno == EWOULDBLOCK){
        log_error("recvfrom() timeout.\n");
        lost++;
      }else{
        log_error("recvfrom() error:%s\n",strerror(errno));
      }
    }
  }while(lost!=0 && lost<4);
  freeaddrinfo(ressave);
  free(dns_query_buf);
  if(lost==4){
    info->resp_code=500;
    info->pro_state=request_done;
    return;
  }
  /*we don't check if transaction id is the same
    and only get the first answer resource record if there's any
    check rcode, then consume to the start of answer section, and then print RDATA to a string*/
  rcode=parse_dns_resp((uint8_t *)dns_resp_buf, num, &rdata_str);
  
  /*format result to http response*/
  create_http_resp_dns(info, rcode, rdata_str);

  free(rdata_str);
}

void process_post_req(struct transaction_info *info){
  char name[MAXHOSTNAME], type[5], dns_result[50];
  memset(name,0,MAXHOSTNAME);
  memset(type,0,5);
  memset(dns_result,0,50);
  
  if(is_dns_query(info->uri)==0){
    info->resp_code=404;
    info->pro_state=request_done;
    return;
  }
  if(parse_dns_query_in_post(info->buf+info->buf_offset, name, type)!=0){
    info->resp_code=400;
    info->pro_state=request_done;
    return;
  }
  /*info->resp_code=200;
  sprintf(info->buf,"name:%s,type=%s\n",name,type);
  info->buf_offset=0;
  info->pro_state=request_done;*/
  handle_dns_query(info, name, type, dns_result);
}

void handle_post_req(struct transaction_info *info){
  if(info->resp_code==200){
    info->pro_state=response_ready;
    return;
  }
  sprintf(info->buf,"HTTP/1.1 %d %s\r\nIam: linzhiqi\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n",info->resp_code,get_resp_reason(info->resp_code),0);
  info->pro_state=response_ready;
}

void serve_http_request(int sockfd, char * doc_root){
  int bytes_read=0;
  char httpMsg[MAXMSGBUF+1], uri[MAX_URI+1], root[MAX_URI+1], file_path[MAX_URI+1];
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
    log_debug("info->pro_state=%d\n",info->pro_state);
    if((bytes_read = readwithtimeout(sockfd, info->buf, MAXMSGBUF,10))<=0){
      log_transaction_state(info, "serve_http_request()-readwithtimeout() error");
      exit(-1);
    }
    info->buf_offset=0;
    info->bytes_in_buf=bytes_read;
    if(info->pro_state==init && parse_req_start_line(info)==-1){
      log_debug("parse_req_start_line(info)==-1\n");
      continue;
    }
    
    if(info->pro_state==start_line_parsed && get_content_length(info)==-1){
      log_debug("get_content_length(info)==-1\n");
      continue;
    }
    
    if(info->pro_state==content_length_got && reach_body(info)==-1){
      log_debug("reach_body(info)==-1\n");
      continue;
    }

    if(info->req_type==put && info->pro_state==body_reached){
      if(info->fd==-1){
        get_fd_for_uri(info);
      }
      if(!info->file_lock_is_got){
        get_file_wlock(info);
      }
      write_data_to_fd(info);
    }else if(info->req_type==post && info->pro_state==body_reached){
      process_post_req(info);
      continue;
    }
  }
  if(info->file_lock_is_got){
    release_file_lock(info);
  }

  if(info->pro_state==request_done){
   /* memset(info->buf,0,MAXMSGBUF+1);*/
    if(info->req_type==put){
      handle_put_req(info);
    }else if(info->req_type==get && is_index_uri(info->uri)){
      handle_index(info);
    }else if(info->req_type==get && !is_index_uri(info->uri)){
      handle_get_req(info);
    }else if(info->req_type==post){
      handle_post_req(info);
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












































