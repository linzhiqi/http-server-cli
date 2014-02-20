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

#define MAXFILENAME 100
#define MAXHOSTNAME 200
#define MAX_URL_LEN 500
#define MAX_PORT_LEN 5
#define MAX_LOCATION_LEN 200
#define MAXMSGBUF 5000
#define DEFAULT_FILE_NAME "downloaded_file"

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
 *host - tageted host name or ip address, support both ipv4 and ipv6
 *serv - service name or port number
 *return - the socket file handler	
 */
int tcp_connect(const char *host, const char *serv)
{
	int				sockfd, n;
	struct addrinfo	hints, *res, *ressave;

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ( (n = getaddrinfo(host, serv, &hints, &res)) != 0) {
		fprintf(stderr, "tcp_connect error for %s, %s: %s\n",
			host, serv, gai_strerror(n));
		return -1;
	}
	ressave = res;

	do {
		sockfd = socket(res->ai_family, res->ai_socktype,
				res->ai_protocol);
		if (sockfd < 0)
			continue;	/* ignore this one */

		if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
			break;		/* success */

		close(sockfd);	/* ignore this one */
	} while ( (res = res->ai_next) != NULL);

	if (res == NULL) {	/* errno set from final connect() */
		fprintf(stderr, "tcp_connect error for %s, %s\n", host, serv);
		sockfd = -1;
	        perror("read error");
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
	long int n=0,total=0,len=0,n1=0,code=0;
	char httpMsg[MAXMSGBUF];
	char *tmp;
        const char *fileName;
	int fd,cnt=0;
	/*in case connection has closed*/
	if(sockfd==-1)
	{
		printf("no site is connected, please connect first\n");
		return 0;
	}

	sprintf(httpMsg,"GET %s HTTP/1.1\r\nHost:%s\r\nIam: linzhiqi\r\n\r\n",res_location,hostName);

	if((n=write(sockfd,httpMsg,strlen(httpMsg)))<0)
	{
		perror("read error");
		return -1;
	}
	if(n<strlen(httpMsg))
	{
		printf("GET message is not sent completely\n");
		return -1;
	}
	/*because some string manipulation is going to take on the buffer, have to mark '\0' following the end of valuable part
	set all byte '\0' so that received part always followed by a '\0'. */
	memset(httpMsg,0,MAXMSGBUF);
	/*parse the http header on the first received part(4999 bytes). assume content-type and empty line will be found here*/
	if((n = read(sockfd, httpMsg, MAXMSGBUF-1))>0)
	{
		printf("get body data in first read(): %ld\n",n);
		/*strstr() returns the pointer pointing to the first char of substring "HTTP/1.1"*/
		tmp=strstr(httpMsg,"HTTP/1.1");
		tmp+=8;
		/*get the response code*/
		code=strtol(tmp,&tmp,10);
		if(code!=200)
		{
			printf("Response not OK, code: %ld\n",code);
			return -1;
		}
		tmp=strstr(httpMsg,"Content-Length");
		tmp+=15;
		/*get the value of content-length*/
		len=strtol(tmp,&tmp,10);
		printf("length:%ld\n",len);
		tmp=strstr(tmp,"\r\n\r\n");
		/*tmp is pointing at the beginning of response body*/
		tmp+=4;
		/*total currently = how many bytes of body data in this buffer needed to be read*/
		total=(httpMsg+n)-tmp;

                if(store_data){
		    /*obtain the file name from resource location, if not specified*/
                    if(local_path==NULL || strcmp(local_path,"")==0){
                        char buf[MAXFILENAME];
                        if(getFileFromRes(buf, res_location)==NULL) fileName=DEFAULT_FILE_NAME;
                        else fileName=buf;
                    }else{
                        fileName=local_path;
                    }

		    if((fd=creat(fileName,S_IRWXO|S_IRWXG|S_IRWXU))==-1)
		    {
			    perror("file create error");
			    return -1;
		    }
                }else{
                    fd=1;
                }

		/*cnt = how many chars needs to write into local file*/
		cnt=(int)total;
		while((n1=write(fd,tmp,(size_t)cnt))>=0)
		{
			if(n1<cnt)
			{
				tmp+=n1;
				cnt-=n1;
			}else{
				break;
			}
		}
		/*if message ends in the first 4999 bytes*/
		if(total>=len)
		{
			close(fd);
			return 0;
		}
	}

	if(n<0)
	{
		perror("read error");
		return -1;
	}else if (n==0){
		printf("connection is closed!\n");
		return -1;
	}

	memset(httpMsg,0,MAXMSGBUF);
	/*if message doesn't end in the first 4999, keep read from the socket*/
	while ( (n = read(sockfd, httpMsg, MAXMSGBUF-1)) > 0) 
	{		
		cnt=n;
		tmp=httpMsg;
		while((n1=write(fd,tmp,(size_t)cnt))>=0)
		{
			if(n1<cnt)
			{
				tmp+=n1;
				cnt-=n1;
			}else{
				break;
			}
		}
		total+=n;
		if(total>=len)
		{
			close(fd);
			return 0;
		}
		memset(httpMsg,0,MAXMSGBUF);
	}

	if (n <0) {
		perror("read error");
		return -1;
	}else if (n==0){
		printf("connection is closed!\n");
		return -1;
	}
	return 0;
}

/*
 *create PUT method and send it with body data read from local file
 */
int upload_file(int sockfd, const char * res_location, const char * hostName, const char *local_path)
{
	long int n=0,len=0,n1=0,code=0;
	char httpMsg[MAXMSGBUF];
	char *tmp=NULL;
	int fd;
	struct stat fileinfo;
	if(sockfd==-1)
	{
		printf("no site is connected, please connect first\n");
		return 0;
	}

	/*get the file size of the object*/
	if(stat(local_path, &fileinfo)==-1)
	{
		perror("file info error");
		return -1;
	}
	len=(long int)fileinfo.st_size;

	sprintf(httpMsg,"PUT %s HTTP/1.1\r\nHost:%s\r\nIam: linzhiqi\r\nContent-Length: %ld\r\n\r\n",res_location,hostName,len);
	if((n=write(sockfd,httpMsg,strlen(httpMsg)))<0)
	{
		perror("read error");
		return -1;
	}
	

	if((fd=open(local_path,O_RDWR))==-1)
	{
		perror("open error");
		return -1;
	}
	/*read file into buffer, then to socket, block by block*/
	while((n1=read(fd,httpMsg,MAXMSGBUF))>0)
	{
		tmp=httpMsg;
		while((n=write(sockfd,tmp,n1))>=0)
		{
                        printf("read data len=%ld, write data len=%ld\n",n1,n);
			if(n<n1)
			{
				tmp+=n;
				n1-=n;
			}else{
				break;
			}
		}
		if(n<0){
			perror("write body");
			return -1;
		}
	}
	/*check the response from server*/
	if((n = read(sockfd, httpMsg, MAXMSGBUF-1))>0)
	{
		tmp=strstr(httpMsg,"HTTP/1.1");
		tmp+=8;
		code=strtol(tmp,&tmp,10);
		if((code/100)==2)
		{
			printf("Put respond OK\n");
			return 0;
		}else{
			printf("Put respond not OK\n");
			return -1;
		}
	}
	if(n1==0)
	{
		printf("connection is closed!\n");
		return -1;
	}else if(n1==-1){
		perror("write to site error");
		return -1;
	}

	
	return 0;
}
