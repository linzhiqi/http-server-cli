all: client server

client: httpdnscli.c lib/logutil.c lib/httputil.c lib/tcputil.c
	gcc -Wall -Wno-overlength-strings -pedantic -pthread -g -o client httpdnscli.c lib/*.c

server: httpserver.c lib/logutil.c lib/httputil.c lib/tcputil.c lib/util.c lib/linkedlist.c lib/dnsutil.c
	gcc -Wall -Wno-overlength-strings -pedantic -pthread -g -o server httpserver.c lib/*.c
