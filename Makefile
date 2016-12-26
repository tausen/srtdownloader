
XMLRPCFLAGS=$(shell xmlrpc-c-config c++2 client --libs)

all:
	gcc -std=c99 -Wall -pedantic -o srtdownloader md5.c main.c opensubtitles.c oshash.c -lcurl -larchive $(XMLRPCFLAGS)
