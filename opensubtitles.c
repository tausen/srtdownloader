#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <archive.h>

#include "opensubtitles.h"
#include "base64.h"
#include "oshash.h"

/****************************************/
/* INTERNAL FUNCTION PROTOTYPES/STRUCTS */
/****************************************/

/* functions for logging in to opensubtitles via xmlrpc */
struct login_response {
    char *token;
    char *status;
    double seconds;
};
struct login_response *login_response_create();
void login_response_destroy(struct login_response *lr);
int parse_login_response(xmlrpc_env *env, xmlrpc_value *resultP, struct login_response *login);

/* functions for searching for subtitle via xmlrpc */
struct search_response {
    char *status;
    char *link;
    int id;
};
struct search_response *search_response_create();
void search_response_destroy(struct search_response *sr);
int parse_search_response(xmlrpc_env *env, xmlrpc_value *resultP, struct search_response *response);

/* functions for downloading subtitle via xmlrpc */
struct download_response {
    char *status;
    char *data;
};
struct download_response *download_response_create();
void download_response_destroy(struct download_response *dr);
int parse_download_response(xmlrpc_env *env, xmlrpc_value *resultP, struct download_response *response);

/* detect error */
bool faultOccured(xmlrpc_env * const envP);

/*********************************/
/* INTERNAL FUNCTION DEFINITIONS */
/*********************************/

struct login_response *login_response_create() {
    struct login_response *ret = malloc(sizeof(struct login_response));
    ret->token = NULL;
    ret->status = NULL;
    return ret;
}

void login_response_destroy(struct login_response *lr) {
    if (lr->token)
        free(lr->token);
    if (lr->status)
        free(lr->status);
}

struct search_response *search_response_create() {
    struct search_response *ret = malloc(sizeof(struct search_response));
    ret->status = NULL;
    ret->link = NULL;
    ret->id = -1;
    return ret;
}

void search_response_destroy(struct search_response *sr) {
    if (sr->link)
        free(sr->link);
    if (sr->status)
        free(sr->status);
}

struct download_response *download_response_create() {
    struct download_response *ret = malloc(sizeof(struct download_response));
    ret->data = NULL;
    ret->status = NULL;
    return ret;
}

void download_response_destroy(struct download_response *sr) {
    if (sr->data)
        free(sr->data);
    if (sr->status)
        free(sr->status);
}

bool faultOccured(xmlrpc_env * const envP) {
    return (bool)envP->fault_occurred;
}

int parse_search_response(xmlrpc_env *env,
                          xmlrpc_value *resultP,
                          struct search_response *response) {
    int i, size = 0;
    xmlrpc_value *el;
    xmlrpc_value *key, *value;
    char *s;
    
    size = xmlrpc_struct_size(env, resultP);
    if (size < 2) {
        fprintf(stderr, "return struct num attrs <2\n");
        return -1;
    }

    /* read status */
    xmlrpc_struct_get_key_and_value(env, resultP, 0, &key, &value);
    if (faultOccured(env))
        return -1;

    xmlrpc_parse_value(env, value, "s", &s);
    if (faultOccured(env))
        return -1;
    response->status = malloc(strlen(s)+1);
    strcpy(response->status, s);
    if (response->status[0] != '2') /* if things go well, expect 200 OK */
        return -1;

    /* read data */
    xmlrpc_struct_get_key_and_value(env, resultP, 1, &key, &value);
    if (faultOccured(env))
        return -1;

    size = xmlrpc_array_size(env, value);
    if (faultOccured(env))
        return -1;
    if (size < 1) {
        /* no results */
        return -1;
    }

    /* grab first result */
    el = xmlrpc_array_get_item(env, value, 0);
    if (faultOccured(env))
        return -1;

    /* parse the result */
    size = xmlrpc_struct_size(env, el);
    if (faultOccured(env))
        return -1;

    for (i = 0; i < size; i++) {
        xmlrpc_struct_get_key_and_value(env, el, i, &key, &value);

        xmlrpc_parse_value(env, key, "s", &s);
        if (faultOccured(env))
            return -1;

        if (strcmp(s, "SubDownloadLink") == 0) {
            xmlrpc_parse_value(env, value, "s", &s);
            if (faultOccured(env))
                return -1;
            response->link = malloc(strlen(s)+1);
            strcpy(response->link, s);
        }

        if (strcmp(s, "IDSubtitleFile") == 0) {
            xmlrpc_parse_value(env, value, "s", &s);
            if (faultOccured(env))
                return -1;
            response->id = atoi(s);
        }
    }

    /* no id attribute found */
    if (response->id == -1)
        return -1;

    return 0;
}

int parse_login_response(xmlrpc_env *env,
                         xmlrpc_value *resultP,
                         struct login_response *login) {
    char* s;
    double d;
    xmlrpc_value *key, *value;
    int size = xmlrpc_struct_size(env, resultP);
    if (faultOccured(env))
        return -1;

    for(int i = 0; i < size; i++) {
        xmlrpc_struct_get_key_and_value(env, resultP, i, &key, &value);
        if (faultOccured(env))
            return -1;

        switch (i) {
        case 0:
        case 1:
            xmlrpc_parse_value(env, value, "s", &s);
            if (faultOccured(env))
                return -1;
            char **sp = (i == 0 ? &login->token : &login->status);
            *sp = malloc(strlen(s)+1);
            strcpy(i == 0 ? login->token : login->status, s);
            break;
        case 2:
            xmlrpc_parse_value(env, value, "d", &d);
            if (faultOccured(env))
                return -1;
            login->seconds = d;
        }
    }

    return 0;
}

int parse_download_response(xmlrpc_env *env,
                            xmlrpc_value *resultP,
                            struct download_response *response) {

    int i, size = 0;
    xmlrpc_value *el;
    xmlrpc_value *key, *value;
    char *s;

    size = xmlrpc_struct_size(env, resultP);
    if (size < 1) {
        fprintf(stderr, "return struct num attrs <1\n");
        return -1;
    }

    xmlrpc_struct_get_key_and_value(env, resultP, 0, &key, &value);
    if (faultOccured(env))
        return -1;

    xmlrpc_parse_value(env, value, "s", &s);
    if (faultOccured(env))
        return -1;
    response->status = malloc(strlen(s)+1);
    strcpy(response->status, s);
    if (response->status[0] != '2')
        return -1;

    xmlrpc_struct_get_key_and_value(env, resultP, 1, &key, &value);
    if (faultOccured(env))
        return -1;

    el = xmlrpc_array_get_item(env, value, 0);
    if (faultOccured(env))
        return -1;

    size = xmlrpc_struct_size(env, el);
    if (faultOccured(env))
        return -1;

    for (i = 0; i < size; i++) {
        xmlrpc_struct_get_key_and_value(env, el, i, &key, &value);

        xmlrpc_parse_value(env, key, "s", &s);
        if (faultOccured(env))
            return -1;

        if (strcmp(s, "data") == 0) {
            xmlrpc_parse_value(env, value, "s", &s);
            if (faultOccured(env))
                return -1;
            response->data = malloc(strlen(s)+1);
            strcpy(response->data, s);
        }
    }

    if (response->data == NULL) {
        fprintf(stderr, "parse download response: no data attr in struct\n");
        return -1;
    }

    return 0;
}

/*********************************/
/* EXTERNAL FUNCTION DEFINITIONS */
/*********************************/
int opensubtitles_get(char *src, char *dest, uint8_t verbosity) {
    int ret = 0;
    xmlrpc_env env;
    xmlrpc_value *resultP = NULL;
    const char *const serverUrl = "https://api.opensubtitles.org/xml-rpc";
    struct login_response *login = login_response_create();
    struct search_response *result = search_response_create();
    struct download_response *download = download_response_create();
    struct archive *a = NULL;
    unsigned char *unpacked = NULL;
    int fd = 0;
    FILE *infile = NULL;

    /* compute hash and grab file size */
    if (verbosity & 0x1)
        fprintf(stderr, "Computing hash...");

    infile = fopen(src, "r");
    if (infile == NULL) {
        fprintf(stderr, "fopen error: %s\n", strerror(errno));
        ret = -1;
        goto end;
    }
    int bytesize;
    uint64_t hash_raw = compute_hash(infile, &bytesize);
    fclose(infile);
    infile = NULL;

    char hash[17]; /* 8x2chars + null terminator */
    sprintf(hash, "%"PRIx64, hash_raw);

    if (verbosity & 0x1)
        fprintf(stderr, "ok (%i bytes, hash: %s)\n", bytesize, hash);

    /* initialize and connect xmlrpc */
    xmlrpc_env_init(&env);

    if (verbosity & 0x1)
        fprintf(stderr, "Logging in...");
    xmlrpc_client_init2(&env, XMLRPC_CLIENT_NO_FLAGS, "srtdownloader", "v0.1", NULL, 0);
    if (faultOccured(&env)) {
        ret = -1;
        goto errout;
    }

    /* using temp agent for testing until we get our own userAgent... */
    // const char * const userAgent = "srtdownloader v0.1";
    resultP = xmlrpc_client_call(&env, serverUrl, "LogIn", "(ssss)",
                                 "", "", "eng", "OSTestUserAgentTemp");
    if (faultOccured(&env)) {
        ret = -1;
        goto errout;
    }

    /* log in */
    if (parse_login_response(&env, resultP, login) < 0) {
        ret = -1;
        xmlrpc_DECREF(resultP);
        goto errout;
    }

    if (login->status[0] != '2') {
        fprintf(stderr, "xmlrpc login error, reply:\n");
        fprintf(stderr, "\ttoken: %s\n\tstatus: %s\n\tseconds: %f\n",
                login->token, login->status, login->seconds);
        ret = -1;
        goto end;
    }

    xmlrpc_DECREF(resultP);

    if (verbosity & 0x1)
        fprintf(stderr, "ok\n");

    if (verbosity & 0x1)
        fprintf(stderr, "Searching...");

    /* search for subtitle */
    resultP = xmlrpc_client_call(&env, serverUrl, "SearchSubtitles", "(s({s:s,s:s,s:i}))",
                                 login->token,
                                 "sublanguageid", "eng",
                                 "moviehash", hash,
                                 "moviebytesize", bytesize);
    if (faultOccured(&env)) {
        ret = -1;
        goto errout;
    }

    if (parse_search_response(&env, resultP, result) < 0) {
        if (result->status[0] != '2')
            fprintf(stderr, "xmlrpc search error: %s\n", result->status);
        ret = -1;
        goto errout;
    }

    xmlrpc_DECREF(resultP);

    if (verbosity & 0x1)
        fprintf(stderr, "ok\n");

    if (verbosity & 0x1)
        fprintf(stderr, "Downloading...");

    /* download subtitle */
    resultP = xmlrpc_client_call(&env, serverUrl, "DownloadSubtitles", "(s(i))",
                                 login->token,
                                 result->id);
    if (faultOccured(&env)) {
        ret = -1;
        goto errout;
    }

    if (parse_download_response(&env, resultP, download) < 0) {
        if (download->status[0] != '2')
            fprintf(stderr, "xmlrpc download error: %s\n", download->status);
        ret = -1;
        goto errout;
    }

    xmlrpc_DECREF(resultP);

    if (verbosity & 0x1)
        fprintf(stderr, "ok\n");

    if (verbosity & 0x1)
        fprintf(stderr, "Base64 decoding...");

    //fprintf(stderr, "download data: %s (%i)\n", download->data, (int)strlen(download->data));

    /* base64 decode */
    int flen;
    unpacked = unbase64(download->data, strlen(download->data), &flen);
    if (unpacked == NULL) {
        fprintf(stderr, "error unpacking base64\n");
        ret = -1;
        goto end;
    }

    if (verbosity & 0x1)
        fprintf(stderr, "ok\n");

    if (verbosity & 0x1)
        fprintf(stderr, "Opening gzip archive...");

    /* initialize libarchive for unpacking gzipped data */
    int r;
    a = archive_read_new();
    if (a == NULL) {
        fprintf(stderr, "archive read new error\n");
        ret = -1;
        goto end;
    }

    r = archive_read_support_filter_all(a);
    if (r != ARCHIVE_OK) {
        fprintf(stderr, "archive filter error: %s\n", archive_error_string(a));
        ret = -1;
        goto end;
    }
    r = archive_read_support_format_all(a);
    if (r != ARCHIVE_OK) {
        fprintf(stderr, "archive format all error: %s\n", archive_error_string(a));
        ret = -1;
        goto end;
    }
    r = archive_read_support_format_raw(a);
    if (r != ARCHIVE_OK) {
        fprintf(stderr, "archive format raw error: %s\n", archive_error_string(a));
        ret = -1;
        goto end;
    }
    r = archive_read_open_memory(a, unpacked, flen);
    if (r != ARCHIVE_OK) {
        fprintf(stderr, "archive open error: %s\n", archive_error_string(a));
        ret = -1;
        goto end;
    }

    if (verbosity & 0x1)
        fprintf(stderr, "ok\n");

    if (verbosity & 0x1)
        fprintf(stderr, "Extracting gzip archive...");

    /* read header */
    struct archive_entry *ae;
    r = archive_read_next_header(a, &ae);
    if (r != ARCHIVE_OK) {
        fprintf(stderr, "archive read header error: %s\n", archive_error_string(a));
        ret = -1;
        goto end;
    }

    /* open destination file for writing */
    fd = open(dest,
              O_CREAT | O_TRUNC | O_WRONLY,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (fd < 0) {
        fprintf(stderr, "fopen error\n");
        ret = -1;
        goto end;
    }

    /* unpack into destination file */
    r = archive_read_data_into_fd(a, fd);
    if (r != ARCHIVE_OK) {
        fprintf(stderr, "archive read data error: %s\n", archive_error_string(a));
        ret = -1;
        goto end;
    }
    close(fd);

    if (verbosity & 0x1)
        fprintf(stderr, "ok\n");

    goto end;
 errout:
    if (faultOccured(&env))
        fprintf(stderr, "ERROR: %s (%d)\n", env.fault_string, env.fault_code);

 end:
    if (verbosity & 0x2)
        fprintf(stderr, "close fd\n");
    if (fd > 0)
        close(fd);

    if (verbosity & 0x2)
        fprintf(stderr, "free unpack\n");
    if (unpacked != NULL) {
        free(unpacked);
    }

    if (verbosity & 0x2)
        fprintf(stderr, "close archive\n");
    if (a != NULL) {
        archive_read_close(a);
        archive_read_free(a);
    }

    if (verbosity & 0x2)
        fprintf(stderr, "destroy\n");
    download_response_destroy(download);
    login_response_destroy(login);
    search_response_destroy(result);
    if (resultP != NULL)
        xmlrpc_DECREF(resultP);

    if (verbosity & 0x2)
        fprintf(stderr, "clean up env\n");

    /* Clean up our error-handling environment. */
    xmlrpc_env_clean(&env);
    
    if (verbosity & 0x2)
        fprintf(stderr, "clean up client\n");
    /* Shutdown our XML-RPC client library. */
    xmlrpc_client_event_loop_finish_asynch();
    // xmlrpc_client_cleanup();
    
    /* NOTE: Getting occasional segfaults when calling client cleanup as above. */
    /* from http://xmlrpc-c.sourceforge.net/doc/libxmlrpc_client.html#client_cleanup */
    /* When you're done using the object, call xmlrpc_client_cleanup(). After that, you
       can start over with a new object by calling xmlrpc_client_init2()
       again. xmlrpc_client_cleanup() destroys the global client and abandons global
       constants (i.e. has the effect of calling xmlrpc_client_teardown_global_const(). In
       truth, this call is unnecessary. All it does is free resources. If your program is
       sloppy enough to use the global client (as opposed to creating a private client of
       its own), it might as well be more sloppy and let the operating system clean up the
       global client automatically as the program exits. */

    /* so: lets be lazy for now, but do things the proper way with a private client some
       day... */

    if (infile != NULL)
        fclose(infile);

    if (verbosity & 0x2)
        fprintf(stderr, "all done\n");

    return ret;
}
