#include <stdint.h>
#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#ifndef OPENSUBTITLES_H_
#define OPENSUBTITLES_H_

int opensubtitles_get(char *src, char *dest, uint8_t verbosity);

#endif
