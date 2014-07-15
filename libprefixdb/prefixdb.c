// This file is part of the PrefixDB library
// Copyright (c) 2014 Pierre-Yves Kerembellec <py.kerembellec@gmail.com>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <libprefixdb.h>

int prefixdb_help()
{
    fprintf
    (
        stderr,
        "usage: prefixdb <action> [<parameters>]\n\n"
        "help                                      show this help screen\n"
        "import <list> <database>                  create a PrefixDB database from a text prefixes list\n"
        "search <database> <address>[ <address>]   search address(es) in a PrefixDB database\n"
        "bench                                     test and bench the installed PrefixDB library\n"
    );
    return 1;
}

#define  SW_START        gettimeofday(&begin, NULL)
#define  SW_END          gettimeofday(&end, NULL)
#define  SW_ELAPSED      ((double)(end.tv_sec - begin.tv_sec) + ((double)(end.tv_usec - begin.tv_usec) / 1000000))
#define  PREFIXES_COUNT  (500000)
#define  SEARCHES_COUNT  (500000)
int prefixdb_bench()
{
    PREFIXDB       *pfdb;
    struct timeval begin, end;
    char           address[30];
    int            exit = 0, status, matches[3], count;

    SW_START; pfdb = prefixdb_allocate(); SW_END;
    printf("allocate empty database   %s [%.06fs]\n", pfdb ? "pass" : "fail", SW_ELAPSED);
    exit |= (!pfdb ? 1 : 0);

    printf("add %6d prefixes       ", PREFIXES_COUNT); fflush(stdout);
    status = 0; srand(time(NULL) + getpid());
    SW_START;
    for (count = 0; count < PREFIXES_COUNT; count ++)
    {
        sprintf(address, "%d.%d.%d.%d/%d", (rand() % 223) + 1, (rand() % 223) + 1, (rand() % 223) + 1, (rand() % 223) + 1, (rand() % 12) + 16);
        status |= prefixdb_add_string(pfdb, address, NULL);
        if (count && !(count % 25000))
        {
            printf("."); fflush(stdout);
        }
    }
    SW_END;
    printf("\radd %6d prefixes       %s [%.06fs] [%d prefixes/s]", PREFIXES_COUNT, status ? "fail" : "pass", SW_ELAPSED, (int)((double)PREFIXES_COUNT / SW_ELAPSED));
    for (count = 0; count < PREFIXES_COUNT / 25000; count ++) printf(" "); printf("\n");
    exit |= (status ? 1 : 0);

    SW_START; status = prefixdb_save_file(pfdb, "/tmp/bench.pfdb"); SW_END;
    printf("save database             %s [%.06fs]\n", status == PREFIXDB_ERROR_OK ? "pass" : "fail", SW_ELAPSED);
    exit |= (status != PREFIXDB_ERROR_OK ? 1 : 0);

    SW_START; status = prefixdb_free(&pfdb); SW_END;
    printf("release database          %s [%.06fs]\n", status == PREFIXDB_ERROR_OK ? "pass" : "fail", SW_ELAPSED);
    exit |= (status != PREFIXDB_ERROR_OK ? 1 : 0);

    SW_START; pfdb = prefixdb_load_file("/tmp/bench.pfdb", 0); SW_END;
    printf("load database             %s [%.06fs]\n", pfdb ? "pass" : "fail", SW_ELAPSED);
    exit |= (!pfdb ? 1 : 0);

    printf("search %6d addresses   ", SEARCHES_COUNT); fflush(stdout);
    matches[0] = matches[1] = matches[2] = 0;
    SW_START;
    for (count = 0; count < SEARCHES_COUNT; count ++)
    {
        sprintf(address, "%d.%d.%d.%d", (rand() % 253) + 1, (rand() % 253) + 1, (rand() % 253) + 1, (rand() % 253) + 1);
        status = prefixdb_search_string(pfdb, address, NULL);
        if      (status == PREFIXDB_ERROR_OK)       matches[0] ++;
        else if (status == PREFIXDB_ERROR_NOTFOUND) matches[1] ++;
        else                                        matches[2] ++;
        if (count && !(count % 25000))
        {
            printf("."); fflush(stdout);
        }
    }
    SW_END;
    printf("\rsearch %6d addresses   %s [%.06fs] [%d searches/s - %d matched - %d unmatched]", SEARCHES_COUNT,
           matches[2] ? "fail": "pass", SW_ELAPSED, (int)((double)SEARCHES_COUNT / SW_ELAPSED), matches[0], matches[1]);
    for (count = 0; count < SEARCHES_COUNT / 25000; count ++) printf(" "); printf("\n");
    exit |= (matches[2] ? 1 : 0);

    SW_START; status = prefixdb_free(&pfdb); SW_END;
    printf("release database          %s [%.06fs]\n", status == PREFIXDB_ERROR_OK ? "pass" : "fail", SW_ELAPSED);
    exit |= (status != PREFIXDB_ERROR_OK ? 1 : 0);

    unlink("/tmp/bench.pfdb");
    return exit;
}

int prefixdb_import(char *list, char *database)
{
    PREFIXDB *pfdb = prefixdb_allocate();

    return prefixdb_add_file(pfdb, list) == PREFIXDB_ERROR_OK &&
           prefixdb_save_file(pfdb, database) == PREFIXDB_ERROR_OK &&
           prefixdb_free(&pfdb) == PREFIXDB_ERROR_OK ? 0 : 1;
}

int prefixdb_search(char *database, char **addresses)
{
   PREFIXDB *pfdb = prefixdb_load_file(database, 0);

   while (*addresses)
   {
       printf("%-15.15s  %s\n", *addresses, prefixdb_search_string(pfdb, *addresses, NULL) == PREFIXDB_ERROR_OK ? "matched": "-");
       addresses ++;
   }
   prefixdb_free(&pfdb);
   return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        return prefixdb_help();
    }
    while (*(argv[1]) == '-') argv[1] ++;
    if (!strncasecmp(argv[1], "help", strlen(argv[1])))
    {
        return prefixdb_help();
    }
    else if (!strncasecmp(argv[1], "bench", strlen(argv[1])))
    {
        return prefixdb_bench();
    }
    else if (!strncasecmp(argv[1], "import", strlen(argv[1])))
    {
        return (argc != 4) ? prefixdb_help() : prefixdb_import(argv[2], argv[3]);
    }
    else if (!strncasecmp(argv[1], "search", strlen(argv[1])))
    {
        return (argc < 4) ? prefixdb_help() : prefixdb_search(argv[2], argv + 3);
    }
    return prefixdb_help();
}
