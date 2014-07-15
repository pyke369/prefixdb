// This file is part of the PrefixDB library
// Copyright (c) 2014 Pierre-Yves Kerembellec <py.kerembellec@gmail.com>

#include <stdint.h>
#include <unistd.h>

#define  PREFIXDB_ERROR_OK        (0)
#define  PREFIXDB_ERROR_PARAM     (1)
#define  PREFIXDB_ERROR_MEMORY    (2)
#define  PREFIXDB_ERROR_ACCESS    (3)
#define  PREFIXDB_ERROR_NOTFOUND  (4)

#define  PREFIXDB_FLAGS_COPY      (0x01)
#define  PREFIXDB_FLAGS_MMAP      (0x02)

typedef  void PREFIXDB;
typedef  void PREFIXDBINFO;

PREFIXDB *prefixdb_allocate();
PREFIXDB *prefixdb_load_file(const char *, uint8_t);
PREFIXDB *prefixdb_load_binary(const uint8_t *, uint32_t, uint8_t);
int      prefixdb_free(PREFIXDB **);
int      prefixdb_add_binary(PREFIXDB *, uint32_t, uint8_t, const PREFIXDBINFO *);
int      prefixdb_add_string(PREFIXDB *, const char *, const PREFIXDBINFO *);
int      prefixdb_add_file(PREFIXDB *, const char *);
int      prefixdb_save_binary(PREFIXDB *, uint8_t **, uint32_t *, uint8_t);
int      prefixdb_save_file(PREFIXDB *, const char *);
int      prefixdb_search_binary(PREFIXDB *, uint32_t, PREFIXDBINFO **);
int      prefixdb_search_string(PREFIXDB *, const char *, PREFIXDBINFO **);
int      prefixdb_free_info(PREFIXDBINFO **);
