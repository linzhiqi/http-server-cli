#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>

int http_server_tosyslog=1;

void log_debug(const char * fmt, ...){
    char buf[1000];
    va_list    ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    if(http_server_tosyslog){
        syslog(LOG_DEBUG, buf);
    }else{
        fprintf(stdout, buf);
    }
}
void log_error(const char * fmt, ...){
    char buf[1000];
    va_list    ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    if(http_server_tosyslog){
        syslog(LOG_ERR, buf);
    }else{
        fprintf(stderr, buf);
    }
}
