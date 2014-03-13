#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>

int http_server_tosyslog=0;
int suppress_debug=0;
int show_thread_id=0;

#define MAXMSG 1500

void log_debug(const char * fmt, ...){
    char buf[MAXMSG], buf2[MAXMSG + 100], *ptr; 
    va_list    ap;

    if(suppress_debug) return;
    ptr=buf;
    va_start(ap, fmt);
    vsnprintf(buf, MAXMSG, fmt, ap);
    va_end(ap);
    if(show_thread_id){
        snprintf(buf2, MAXMSG + 100, "thread-%d %s", (int)pthread_self(), buf);
        ptr=buf2;
    }
    if(http_server_tosyslog){
        syslog(LOG_DEBUG, "%s", ptr);
    }else{
        fprintf(stdout, "%s", ptr);
    }
}
void log_error(const char * fmt, ...){
    char buf[MAXMSG], buf2[MAXMSG + 100], *ptr; 
    va_list    ap;
    
    ptr=buf;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    if(show_thread_id){
        snprintf(buf2, MAXMSG + 100, "thread-%d %s", (int)pthread_self(), buf);
        ptr=buf2;
    }
    if(http_server_tosyslog){
        syslog(LOG_ERR, "%s", ptr);
    }else{
        fprintf(stderr, "%s", ptr);
    }
}
