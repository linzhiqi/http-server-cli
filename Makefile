make: httpdnscli.c httputil.c
	gcc -Wall -Wno-overlength-strings -pedantic -g -o dnscli httpdnscli.c httputil.c httputil.h
