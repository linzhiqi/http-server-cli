#ifndef NWPROG_LINZHIQI_HTTPUTIL_H
#define NWPROG_LINZHIQI_HTTPUTIL_H


#define MAXHOSTNAME 200
#define MAX_URL_LEN 500
#define MAX_PORT_LEN 5
#define MAX_LOCATION_LEN 200

enum processing_state{ init, start_line_parsed, content_length_got, body_reached, request_done, response_ready, response_sent };
enum request_type{ unsupported, get, put, post };
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
