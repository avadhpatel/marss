/* core-dump-handler.c */

/*
 * This file provides interface to dump application core-dump to Host via
 * 'ptlcall'. Use following steps to setup core-dump handler:
 *
 * 1. Compile this file:
 *      $ gcc -O3 core-dump-handler.c -o core-dump-handler
 *
 * 2. Copy the binary to VM' /bin directory
 * 3. Modify VM's /proc/sys/kernel/core_pattern by follwoing command:
 *      # echo "|/bin/core-dump-handler %s %e" > /proc/sys/kernel/core_pattern
 *
 *  Note: Add a startup-script so after every boot core-dump-handler is set.
 *
 */

#define _GNU_SOURCE
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define PTLCALLS_USERSPACE
#include "ptlcalls.h"

#define BUF_SIZE 1024
#define DUMP_BLK_SIZE 1024*1024*500

int
main(int argc, char *argv[])
{
    int      tot, j;
    ssize_t  nread;
    char     buf[BUF_SIZE];
    char    *app_name;
    char    *dump;
    char    *dump2;
    int      dump_size;
    int      signum;

    /* First argument is Signal number */
    signum = atoi(argv[1]);

    /* Second argument is Application name */
    app_name = (char*)malloc(sizeof(char) * strlen(argv[2]));
    app_name = strcpy(app_name, argv[2]);

    /* Copy all the core-dump information into local buffer and then pass that
     * buffer to Host via ptlcall.
     */

    tot       = 0;
    dump      = NULL;
    dump2     = NULL;
    dump_size = 0;
    while ((nread = read(STDIN_FILENO, buf, BUF_SIZE)) > 0) {

        if (dump_size < tot + nread) {
            dump_size += DUMP_BLK_SIZE;
            dump = (char*)realloc(dump, dump_size);
            if (dump == NULL) {
                printf("Can't allocate enough memory for core-dump\n");
                exit(EXIT_FAILURE);
            }

            dump2 = dump + tot;
        }

        dump2 = memcpy(dump2, buf, nread);

        tot   += nread;
        dump2 += nread;
    }

    ptlcall_core_dump(dump, tot, app_name, signum);

    exit(EXIT_SUCCESS);
}
