#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>

int http_server_tosyslog=0;
int suppress_debug=0;

void log_debug(const char * fmt, ...){
    char buf[1000];  
    va_list    ap;

    if(suppress_debug) return;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    if(http_server_tosyslog){
        syslog(LOG_DEBUG, "%s", buf);
    }else{
        fprintf(stdout, "%s", buf);
    }
}
void log_error(const char * fmt, ...){
    char buf[1000];
    va_list    ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    if(http_server_tosyslog){
        syslog(LOG_ERR, "%s", buf);
    }else{
        fprintf(stderr, "%s", buf);
    }
}
