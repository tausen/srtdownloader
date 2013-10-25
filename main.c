/*
 * This is a simple command-line subtitle downloader.
 
 * Subtitles are downloaded from thesubdb.com using a md5 hash of the video
 * file. Refer to http://thesubdb.com/api/ for more information about thesubdb
 * and the API. The md5 hash is calculated using the OpenSSL-compatible
 * implementation by Alexander Peslyak, refer to md5.c and md5.h for license and
 * more information.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <curl/curl.h>
#include <curl/easy.h>

#include "md5.h"

#define READSIZE 64*1024L

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written;
    written = fwrite(ptr, size, nmemb, stream);
    return written;
}

/* http://stackoverflow.com/questions/2736753/how-to-remove-extension-from-file-name */
char *remove_ext (char* mystr, char dot, char sep) {
    char *retstr, *lastdot, *lastsep;

    // Error checks and allocate string.

    if (mystr == NULL)
        return NULL;
    if ((retstr = malloc (strlen (mystr) + 4)) == NULL) // +4 for null char and ".srt"
        return NULL;

    // Make a copy and find the relevant characters.

    strcpy (retstr, mystr);
    lastdot = strrchr (retstr, dot);
    lastsep = (sep == 0) ? NULL : strrchr (retstr, sep);

    // If it has an extension separator.

    if (lastdot != NULL) {
        // and it's before the extenstion separator.

        if (lastsep != NULL) {
            if (lastsep < lastdot) {
                // then remove it.

                *lastdot = '\0';
            }
        } else {
            // Has extension separator with no path separator.

            *lastdot = '\0';
        }
    }

    // Return the modified string.

    return retstr;
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Not enough arguments!\nUsage: srtdownloader <videofile>\n");
        return 0;
    }

    /* Open the file for reading */
    FILE *rf = fopen(argv[1], "rb");
    if (rf == NULL) {
        printf("Could not read file: %s\n", strerror(errno));
        return 1;
    }

    /* Get the full filename and add srt extension */
    char *fname = remove_ext (argv[1], '.', '/');
    sprintf(fname, "%s.srt", fname);
    
    /* Put the first and last 64 kbyte of the file in buffer */
    uint8_t buffer[READSIZE*2];
    
    fread(buffer, 1, READSIZE, rf);
    fseek(rf, READSIZE*(-1), SEEK_END);
    fread(&buffer[READSIZE], 1, READSIZE, rf);
    fclose(rf);

    /* Generate md5 hash of buffer */
    uint8_t md5sum[16];
    MD5_CTX context;
    MD5_Init(&context);
    MD5_Update(&context, (const char*)buffer, READSIZE*2);
    MD5_Final(md5sum, &context);

    /* Convert md5 hash to string */
    char md5string[33];
    for(int i = 0; i < 16; ++i)
        sprintf(&md5string[i*2], "%02x", (unsigned int)md5sum[i]);

    /* Download subtitle using curl */
    CURL *curl;
    FILE *fp;
    char url[255];
    sprintf(url, "http://api.thesubdb.com/?action=download&hash=%s&language=en", md5string);
    char *user_agent = "SubDB/1.0 (srtdownloader/0.1; https://github.com/tausen/srtdownloader)";
    curl = curl_easy_init();
    if (curl) {
        printf("Downloading subtitle to %s... ", fname);
        fp = fopen(fname,"wb");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        CURLcode res = curl_easy_perform(curl);
        if (res == 0) {
            printf("done\n");
        } else {
            printf("FAILED\n");
        }
        /* cleanup */
        curl_easy_cleanup(curl);
        fclose(fp);
    } else {
        printf("Could not initialize curl - what did you do?\n");
    }

    free(fname);

    return 0;
}
