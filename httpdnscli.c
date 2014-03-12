#include "lib/httputil.h"
#include "lib/tcputil.h"

#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int c, uflg=0, iflg=0, oflg=0;
    int sockfd=-1;
    extern char *optarg;
    extern int optind, optopt;
    char *url, *filePath;
    char port[MAX_PORT_LEN], host[MAXHOSTNAME], location[MAX_LOCATION_LEN];

    char * usage_msg = 
"\nUsage: ./dnscli <-u> [-i] [-o]\n\
    -u    the url to request. Argument is mandatory.\n\
          With -u option alone, it fetches content and print to console only.\n\
    -i    the local path to store the data fetched. \n\
          Argument is optional and use the resource file name by default\n\
    -o    the local path of the file to be upload. Argument is mandatory\n\
    -h    show the usage\n\
Example1: ./dnscli -u nwprog1.netlab.hut.fi:3000/index\n\
Example2: ./dnscli -u nwprog1.netlab.hut.fi:3000/upload_011 -i my_download.txt\n\
Example3: ./dnscli -u nwprog1.netlab.hut.fi:3000/upload_012 -o my_download.txt\n\
url:=[protocol://]+<hostname>+[:port number]+[resource location]\n\n";

    while ((c = getopt(argc, argv, ":hu:i:o:")) != -1) {
        
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
    sockfd=tcp_connect(host,port);
    if(sockfd==-1){
        printf("tcp_connect() failed!\n");
        return -1;
    }
    if(iflg>0){
        if(fetch_body(sockfd,location,host,filePath,1)==0){
	    printf("download success!\n");
	}else{
	    printf("download fail!\n");
        }
    }else if(oflg>0){
        if(upload_file(sockfd,location,host,filePath)==0){
	    printf("upload success!\n");
	}else{
	    printf("upload fail!\n");
        }
    }else{
        if(fetch_body(sockfd,location,host,filePath,0)==0){
	    printf("show content success!\n");
	}else{
	    printf("show content fail!\n");
        }
    }

    return 0;
}



