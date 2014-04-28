#include "lib/httputil.h"
#include "lib/tcputil.h"
#include "lib/logutil.h"
#include "lib/util.h"

#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#define MAX_POST_BODY_SIZE 500
#define MAXFILENAME 300

void query_host_name(int sockfd, const char * uri, const char * host, const char * query_name, const char *qtype);
void do_it();
void *func(void *arg);

char *url, *filePath, *hostName, *type;
char port[MAX_PORT_LEN], host[MAXHOSTNAME], location[MAX_LOCATION_LEN];
int uflg=0, iflg=0, oflg=0, dflg=0, tflg=0, num_thread=1;

int main(int argc, char *argv[])
{
  int c,i,ret=0;
  extern char *optarg;
  extern int optind, optopt;
  pthread_t *tid;

  char * usage_msg = 
"\nUsage: ./dnscli <-u> [-i] [-o] [-d [-t]] [-n] [-h]\n\
    -u    the url to request. Argument is mandatory.\n\
          With -u option alone, it fetches content and print to console only.\n\
    -i    the local path to store the data fetched. \n\
          Argument is optional and use the resource file name by default\n\
    -o    the local path of the file to be upload. Argument is mandatory\n\
    -d    the host name to be queried by http DNS server\n\
    -t    the type of DNS query, only \"A\" and \"AAAA\" is supported, case ignored. \"A\" by default\n\
    -n    number of parallel requests\n\
    -h    show the usage\n\
Example1: ./client -u nwprog1.netlab.hut.fi:3000/index\n\
Example2: ./client -u nwprog1.netlab.hut.fi:3000/upload_011 -i my_download.txt\n\
Example3: ./client -u nwprog1.netlab.hut.fi:3000/upload_012 -o my_download.txt\n\
Example4: ./client -u nwprog3.netlab.hut.fi:3333/dns-query -d nwprog1.netlab.hut.fi\n\
Example5: ./client -u nwprog3.netlab.hut.fi:3333/dns-query -d nwprog1.netlab.hut.fi -t AAAA\n\
url:=[protocol://]+<hostname>+[:port number]+[resource location]\n\n";

  while ((c = getopt(argc, argv, ":hu:i:o:d:t:n:")) != -1) {
        
    switch(c) {
      case 'u':
        uflg++;
        url = optarg;
        printf("The targeted url is %s\n", url);
        break;
      case 'i':
        iflg++;
        filePath = optarg;
        printf("Downloaded file will be stored at %s\n", filePath);
        break;
      case 'o':
        oflg++;
        filePath = optarg;
        printf("File to be upload is stored at %s\n", filePath);
        break;
      case 'd':
        dflg++;
        hostName = optarg;
        printf("The host name to be query: %s\n", hostName);
        break;
      case 't':
        tflg++;
        type = optarg;
        printf("The Type of the DNS query: %s\n", type);
        break;
      case 'n':
        num_thread = (int)strtol(optarg,&optarg,10);
        printf("Number of parallel requests: %d\n", num_thread);
        break;
      case 'h':
        printf("%s",usage_msg);
	exit(0);
      case ':':
        if(optopt=='i'){
          filePath=NULL;
          iflg++;
        }else{
          printf("-%c argument is missing\n", optopt);
        }
        break;
      case '?':
        printf("Unknown arg %c\n", optopt);
        break;
      }
        
  }

  if(uflg<=0)
  {
    printf("%s",usage_msg);
    exit(0);
  }else{
    parse_url(url, port, host, location);
  }

  tid=(pthread_t *)malloc(num_thread*sizeof(pthread_t));
  for(i=0;i<num_thread;i++){
    ret=pthread_create(&tid[i], NULL, &func, (void *)NULL);
    log_debug("new thread-%d is created. pthread_create() return %d\n",(int)tid[i], ret);
  }

  for(i=0;i<num_thread;i++){
    pthread_join(tid[i], NULL);
  }
  free(tid);
  return 0;
}

void *func(void *arg)
{
  struct timeval start, end;
  double time_elapsed;
  gettimeofday(&start, NULL);
  do_it();
  gettimeofday(&end, NULL);
  time_elapsed = getTimeElapsed(end, start);
  log_debug("thread %d finished in %f seconds\n",(int)pthread_self(),time_elapsed);
  return(NULL);
}

void do_it(){
  int sockfd=-1;
  char local_path[MAXFILENAME+1];
  local_path[MAXFILENAME]='\0';
  sockfd=tcp_connect(host,port);
  if(sockfd==-1){
    log_debug("tcp_connect() failed!\n");
    return;
  }
  if(iflg>0){
    if(num_thread>1){
      snprintf(local_path,MAXFILENAME,"%s-%d",filePath,(int)pthread_self());
    }else{
      snprintf(local_path,MAXFILENAME,"%s",filePath);
    }
    if(fetch_body(sockfd,location,host,local_path,1)==0){
      log_debug("download success!\n");
    }else{
      log_debug("download fail!\n");
    }
  }else if(oflg>0){
    if(upload_file(sockfd,location,host,filePath)==0){
      log_debug("upload success!\n");
    }else{
      log_debug("upload fail!\n");
    }
  }else if(dflg>0){
    if(tflg==0){
      type="A";
    }
    query_host_name(sockfd,location,host,hostName,type);

  }else{
    if(fetch_body(sockfd,location,host,filePath,0)==0){
      log_debug("show content success!\n");
    }else{
      log_debug("show content fail!\n");
    }
  }
}

/*request http server "host" with uri "uri" for DNS answer of "query_name" of "qtype"*/
void query_host_name(int sockfd, const char * uri, const char * host, const char * query_name, const char *qtype){
  char body[MAX_POST_BODY_SIZE];
  memset(body,0,MAX_POST_BODY_SIZE);
  snprintf(body,MAX_POST_BODY_SIZE-1,"Name=%s&Type=%s", query_name,qtype);
  post_transaction(sockfd, uri, host, body);
}



