all:
	gcc -std=c99 -Wall -pedantic -o srtdownloader md5.c main.c -lcurl
