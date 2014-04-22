#ifndef NWPROG_LINZHIQI_HTTPUTIL_H
#define NWPROG_LINZHIQI_HTTPUTIL_H


#define MAXHOSTNAME 200
#define MAX_URL_LEN 500
#define MAX_PORT_LEN 5
#define MAX_LOCATION_LEN 200


void parse_url(const char *url, char *port, char *host, char *location);
char *getFileFromRes(char * file_name, const char * res_location);
void post_transaction(int sockfd, const char * uri, const char * host, const char * body);
int fetch_body(int sockfd, char * res_location, const char * hostName, const char *local_path, int store_data);
int upload_file(int sockfd, const char * res_location, const char * hostName, const char *local_path);

int parse_req_line(const char * req_line, char * method_ptr, char * uri_ptr);
void serve_put(int sockfd, const char * root_path, const char * uri_ptr);
void serve_get(int connfd, const char * root_path, const char * uri_ptr);
void serve_http_request(int sockfd, char * doc_root);
#endif
