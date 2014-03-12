all: client httpserver

client: httpdnscli.c lib/logutil.c lib/httputil.c lib/tcputil.c
	gcc -Wall -Wno-overlength-strings -pedantic -pthread -g -o dnscli httpdnscli.c lib/*.c

httpserver: httpserver.c lib/logutil.c lib/httputil.c lib/tcputil.c lib/util.c lib/linkedlist.c
	gcc -Wall -Wno-overlength-strings -pedantic -pthread -g -o httpserver httpserver.c lib/*.c
