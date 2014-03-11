#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <syslog.h>
#include <stdarg.h>
#include <netdb.h>
#include <arpa/inet.h>

#define LISTENQ 5
#define MAXCONNECTION 50

int daemon_init(const char *pname, int facility);
extern int http_server_tosyslog=1;
void start_server(const char* process_name, const char * port, const char * root_path, const int debugflg);
void *serve(void *arg);


extern struct node * fileLinkedList;
extern char *root_path, *process_name, *port;

int main(int argc, char *argv[])
{
    int c, portflg=0, debugflg=0;
    extern char *optarg;
    extern int optind, optopt;

    char * usage_msg = 
"\nUsage: ./httpserver [-n] <-p> [-r] [-d] [-h]\n\
    -n    the process name. Argument is mandatory.\n\
    -p    the port it listens on. Argument is mandatory.\n\
    -d    debug model if this is specified.\n\
    -r    document root, the working directory by default\n\
    -h    show the usage\n\
Example1: ./httpserver -p 3000\n\
Example2: ./httpserver -p 3000 -d\n\
Example3: ./httpserver -p 3000 -r /home/NAME/my_server_root/\n\n";

    while ((c = getopt(argc, argv, ":hdn:p:r:")) != -1) {
        
        switch(c) {
        case 'n':
            process_name = optarg;
            printf("process name: %s.\n", process_name);
            break;
        case 'p':
            portflg++;
            port = optarg;
            printf("port: %s.\n", port);
            break;
        case 'r':
            root_path = optarg;
            printf("document root: %s.\n", root_path);
            break;
        case 'd':
            http_server_tosyslog=0;
            debugflg++;
            printf("debug mode is enabled.\n");
            break;
        case 'h':
            printf("%s",usage_msg);
	    exit(0);
        case ':':
            if(optopt=='r'){
                root_path=NULL;
            }else if(optopt=='n'){
                process_name="http-server-zhiqi";
            }else{
            printf("-%c argument is missing\n", optopt);
            }
            break;
        case '?':
            printf("Unknown arg %c\n", optopt);
            exit(0);
        }
        
    }

    if(portflg<=0)
    {
        printf("%s",usage_msg);
	exit(0);
    }else{
        start_server(process_name,port,root_path,debugflg);
    }

    return 0;
}




void start_server(const char* process_name, const char * port, const char * root_path, const int debugflg){
    struct sockaddr    *cliaddr;
    int    i, listenfd;
    pthread_t tid;
    int connfd[MAXCONNECTION];
    socklen_t    clilen, addrlen;
    fprintf(stdout, "pid-%d started...\n",(int)getpid());
    if(debugflg<1){
        daemon_init(process_name,LOG_WARNING);
        sleep(3);
    }
 
    log_debug("HttpServer-%d started...\n",(int)getpid());
    listenfd = tcp_listen(NULL, port, &addrlen);

    cliaddr = malloc(addrlen);
    for( i=0; i < MAXCONNECTION; i++)
    {
        connfd[i] = -1;
    }
    for ( ; ; ) {
        int j=0;
	
        clilen = addrlen;
        for(j=0; j < MAXCONNECTION; j++)
        {
            if(connfd[j] == -1)
            {
                break;
            }	
        }
		
        connfd[j] = accept(listenfd, cliaddr, &clilen);
        log_debug("Accepted a new client from %s\n",sock_ntop(cliaddr,clilen));
        pthread_create(&tid, NULL, &serve, (void *) &(connfd[j]));
    }
}

void serve_process(int connfd)
{
    char httpMsg[MAXMSGBUF], method[20], uri[MAX_LOCATION_LEN];
    int req_line_flg=0;
    log_debug("I am %d, my socket descriptor is %d, bye!\n",(int)getpid(), connfd);

    memset(httpMsg,0,MAXMSGBUF);
    readline_timeout(connfd, httpMsg, 10);
    /*read request line*/ 
    if(parse_req_line(httpMsg, method, uri)==-1){
        /*send erro response*/
        return;
    }

    switch(method){
        case "GET":
            /*serve GET*/
            serve_get(connfd, uri);
            break;
        case "PUT":
            /*serve PUT*/
            serve_put(connfd, uri);
            break;
        default:
            /*this method is not supported*/
            break;
    }
}

void *serve(void *arg)
{
	/*	void	web_child(int);*/

	pthread_detach(pthread_self());
	serve_process(*((int *) arg));
	close(*((int*) arg));
	*((int*) arg) = -1;
	return(NULL);
}























































