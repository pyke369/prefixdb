// This file is part of the PrefixDB library
// Copyright (c) 2014 Pierre-Yves Kerembellec <py.kerembellec@gmail.com>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <libprefixdb.h>

#define  PREFIXDB_LIBRARY_VERSION  (0x0101)
#define  PREFIXDB_MAGIC_MARKER     (0x50464442)
#define  PREFIXDB_FLAGS_SERIALIZED (0x80)

typedef struct __PREFIXDB_NODE
{
    struct __PREFIXDB_NODE *down[2], *up;
    uint32_t               id, explored[2], flaggued;
} _PREFIXDB_NODE;

typedef struct
{
    _PREFIXDB_NODE nodes;
    uint32_t       version, nodes_count, data_size, pass;
    uint8_t        *data, records_size, flags;
    int            handle;
} _PREFIXDB;

PREFIXDB *prefixdb_allocate()
{
    return (PREFIXDB *)calloc(1, sizeof(_PREFIXDB));
}

PREFIXDB *prefixdb_load_binary(const uint8_t *data, uint32_t size, uint8_t flags)
{
    _PREFIXDB *db;
    uint32_t  nodes_count, version;
    uint8_t   records_size;

    if (!data || size < 31 || memcmp(data + size - 31, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16) ||
        ntohl(*(uint32_t *)(data + size - 4)) != PREFIXDB_MAGIC_MARKER || ntohl(*(uint32_t *)(data + size - 8)) != size ||
        (version = ntohs(*(uint16_t *)(data + size - 10))) > PREFIXDB_LIBRARY_VERSION ||
        (nodes_count = ntohl(*(uint32_t *)(data + size - 14))) == 0xffffffff || (records_size = *(data + size - 15)) % 8 ||
        (nodes_count * (records_size / 8) * 2) != (size - 31) || !(db = prefixdb_allocate()))
    {
        return NULL;
    }
    db->records_size = records_size / 8;
    db->nodes_count  = nodes_count;
    db->data_size    = size;
    db->flags       |= PREFIXDB_FLAGS_SERIALIZED;
    if (flags & PREFIXDB_FLAGS_COPY)
    {
        db->flags |= PREFIXDB_FLAGS_COPY;
        if (!(db->data = (uint8_t *)malloc(db->data_size)))
        {
            prefixdb_free((PREFIXDB *)&db);
            return NULL;
        }
        memcpy(db->data, data, size);
    }
    else
    {
        db->data = (uint8_t *)data;
    }
    return db;
}

PREFIXDB *prefixdb_load_file(const char *path, uint8_t flags)
{
    _PREFIXDB   *db;
    struct stat info;
    uint32_t    nodes_count, version;
    uint8_t     data[31], records_size;
    int         handle;

    if (!path || stat(path, &info) < 0 || info.st_size < 31 || (handle = open(path, O_RDONLY)) < 0)
    {
        return NULL;
    }
    if (lseek(handle, info.st_size - 31, SEEK_SET) != (info.st_size - 31) || read(handle, data, 31) != 31 ||
        memcmp(data, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16) ||
        ntohl(*(uint32_t *)(data + 27)) != PREFIXDB_MAGIC_MARKER || ntohl(*(uint32_t *)(data + 23)) != info.st_size ||
        (version = ntohs(*(uint16_t *)(data + 21))) > PREFIXDB_LIBRARY_VERSION ||
        (nodes_count = ntohl(*(uint32_t *)(data + 17))) == 0xffffffff || (records_size = *(data + 16)) % 8 ||
        (nodes_count * (records_size / 8) * 2) != (info.st_size - 31) || !(db = prefixdb_allocate()))
    {
        close(handle);
        return NULL;
    }
    db->records_size = records_size / 8;
    db->nodes_count  = nodes_count;
    db->data_size    = info.st_size;
    db->flags       |= PREFIXDB_FLAGS_SERIALIZED;
    if (flags & PREFIXDB_FLAGS_MMAP)
    {
        db->flags |= PREFIXDB_FLAGS_MMAP;
        db->handle = handle;
        if ((db->data = (uint8_t *)mmap(NULL, db->data_size, PROT_READ, MAP_SHARED, db->handle, 0)) == MAP_FAILED)
        {
            close(handle);
            prefixdb_free((PREFIXDB *)&db);
            return NULL;
        }
    }
    else
    {
        db->flags |= PREFIXDB_FLAGS_COPY;
        lseek(handle, 0, SEEK_SET);
        if (!(db->data = (uint8_t *)malloc(db->data_size)) || (read(handle, db->data, db->data_size) != db->data_size))
        {
            close(handle);
            if (db->data) free(db->data);
            prefixdb_free((PREFIXDB *)&db);
            return NULL;
        }
        close(handle);
    }
    return db;
}

static int prefixdb_free_down(_PREFIXDB_NODE *node)
{
    _PREFIXDB_NODE *pnode, *unode;

    pnode = node;
    while (1)
    {
        while (pnode->down[0])
        {
            pnode = pnode->down[0];
        }
        if (pnode->down[1])
        {
            pnode = pnode->down[1];
            continue;
        }
        if (pnode == node)
        {
            break;
        }
        unode = pnode->up;
        unode->down[unode->down[0] == pnode ? 0 : 1] = NULL;
        free(pnode);
        pnode = unode;
    }
    return PREFIXDB_ERROR_OK;
}

int prefixdb_free(PREFIXDB **_db)
{
    _PREFIXDB *db;

    if (!_db || !(db = (_PREFIXDB *)*_db))
    {
        return PREFIXDB_ERROR_PARAM;
    }
    prefixdb_free_down(&(db->nodes));
    if (db->data)
    {
        if (db->flags & PREFIXDB_FLAGS_COPY)
        {
            free(db->data);
        }
        else if (db->flags & PREFIXDB_FLAGS_MMAP)
        {
            munmap(db->data, db->data_size);
            close(db->handle);
        }
    }
    free(*_db);
    return PREFIXDB_ERROR_OK;
}

int prefixdb_add_binary(PREFIXDB *_db, uint32_t address, uint8_t length, const PREFIXDBINFO *__info)
{
    _PREFIXDB_NODE *pnode, *anode;
    _PREFIXDB      *db = (_PREFIXDB *)_db;
    int8_t         bit, type = 0, allocated = 0;

    if (!db || length <= 0 || length > 32)
    {
        return PREFIXDB_ERROR_PARAM;
    }
    db->flags &= ~PREFIXDB_FLAGS_SERIALIZED;
    pnode = &(db->nodes);
    for (bit = 31; bit > 31 - length; bit --)
    {
        type = (address & (1 << bit)) ? 1 : 0;
        if (!(pnode->down[type]))
        {
            if (!(pnode->down[type] = anode = (_PREFIXDB_NODE *)calloc(1, sizeof(_PREFIXDB_NODE))))
            {
                return PREFIXDB_ERROR_MEMORY;
            }
            anode->up = pnode;
            allocated = 1;
        }
        pnode = pnode->down[type];
        if (!allocated && !(pnode->down[0]) && !(pnode->down[1])) // longer prefix redux
        {
            break;
        }
        allocated = 0;
    }
    prefixdb_free_down(pnode); // shorter prefix redux
    return PREFIXDB_ERROR_OK;
}

int prefixdb_add_string(PREFIXDB *_db, const char *prefix, const void *__info)
{
    struct in_addr address;
    uint8_t        length = 32;
    char           line[64], *token;

    if (!prefix)
    {
        return PREFIXDB_ERROR_PARAM;
    }
    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line), "%s", prefix);
    if ((token = strchr(line, '/')))
    {
        *token = 0;
        length = atoi(token + 1);
    }
    if (!inet_aton(line, &address))
    {
        return PREFIXDB_ERROR_PARAM;
    }
    return prefixdb_add_binary(_db, htonl(address.s_addr), length, __info);
}

int prefixdb_add_file(PREFIXDB *_db, const char *path)
{
    FILE *input;
    int  status;
    char line[64], *token;

    if (!path || !(input = fopen(path, "r")))
    {
        return PREFIXDB_ERROR_PARAM;
    }
    while (fgets(line, sizeof(line) - 1, input))
    {
        if ((token = strpbrk(line, "#\r\n")))
        {
            *token = 0;
        }
        if (*line)
        {
            if ((status = prefixdb_add_string(_db, line, NULL)) != PREFIXDB_ERROR_OK)
            {
                break;
            }
        }
    }
    fclose(input);
    return status;
}

static int prefixdb_write_node(_PREFIXDB *db, uint32_t node, uint32_t value0, uint32_t value1)
{
    uint8_t *base = db->data + (node * 2 * db->records_size), count;

    if (!db || !db->data || node > db->nodes_count || value0 >= db->data_size || value1 >= db->data_size)
    {
        return PREFIXDB_ERROR_PARAM;
    }
    count = db->records_size;
    while (count)
    {
        *(base + count - 1) = (uint8_t)(value0 & 0xff);
        value0 >>= 8;
        count --;
    }
    count = db->records_size;
    while (count)
    {
        *(base + db->records_size + count - 1) = (uint8_t)(value1 & 0xff);
        value1 >>= 8;
        count --;
    }
    return PREFIXDB_ERROR_OK;
}

static int prefixdb_serialize(_PREFIXDB *db)
{
    _PREFIXDB_NODE *pnode;
    uint32_t       count;

    if (db->flags & PREFIXDB_FLAGS_SERIALIZED)
    {
        return PREFIXDB_ERROR_OK;
    }
    db->pass += 5;

    // prefixes redux (pass 1)
    pnode = &(db->nodes);
    while (1)
    {
        if (pnode->down[0] && !pnode->down[0]->down[0] && !pnode->down[0]->down[1] &&
            pnode->down[1] && !pnode->down[1]->down[0] && !pnode->down[1]->down[1])
        {
            free(pnode->down[0]);
            free(pnode->down[1]);
            pnode->down[0] = pnode->down[1] = NULL;
        }
        while (pnode->down[0] && pnode->explored[0] != (db->pass + 1))
        {
            pnode->explored[0] = (db->pass + 1);
            pnode              = pnode->down[0];
        }
        if (pnode->down[1] && pnode->explored[1] != (db->pass + 1))
        {
            pnode->explored[1] = (db->pass + 1);
            pnode              = pnode->down[1];
            continue;
        }
        if (pnode == &(db->nodes))
        {
            break;
        }
        pnode = pnode->up;
    }

    // nodes numbering (pass 2)
    pnode = &(db->nodes);
    db->nodes_count = 0;
    while (1)
    {
        if (pnode->flaggued != (db->pass + 1))
        {
            pnode->flaggued = (db->pass + 1);
            pnode->id       = 0;
            if (pnode->down[0] || pnode->down[1])
            {
                pnode->id = db->nodes_count ++;
            }
        }
        while (pnode->down[0] && pnode->explored[0] != (db->pass + 2))
        {
            pnode->explored[0] = (db->pass + 2);
            pnode              = pnode->down[0];
            if (pnode->flaggued != (db->pass + 1))
            {
                pnode->flaggued = (db->pass + 1);
                pnode->id       = 0;
                if (pnode->down[0] || pnode->down[1])
                {
                    pnode->id = db->nodes_count ++;
                }
            }
        }
        if (pnode->down[1] && pnode->explored[1] != (db->pass + 2))
        {
            pnode->explored[1] = (db->pass + 2);
            pnode              = pnode->down[1];
            continue;
        }
        if (pnode == &(db->nodes))
        {
            break;
        }
        pnode = pnode->up;
    }

    if (db->data)
    {
        if (db->flags & PREFIXDB_FLAGS_COPY)
        {
            free(db->data);
        }
        else if (db->flags & PREFIXDB_FLAGS_MMAP)
        {
            munmap(db->data, db->data_size);
            close(db->handle);
        }
    }

    db->records_size = 0;
    count            = db->nodes_count + 16;
    do
    {
        db->records_size ++;
    } while (count /= 256);
    db->data_size = ((2 * db->records_size) * db->nodes_count) + 31;
    if (!(db->data = (uint8_t *)calloc(1, db->data_size)))
    {
        return PREFIXDB_ERROR_MEMORY;
    }
    db->flags |= (PREFIXDB_FLAGS_COPY | PREFIXDB_FLAGS_SERIALIZED);
    *((uint32_t *)(db->data + db->data_size - 4))  = htonl(PREFIXDB_MAGIC_MARKER);
    *((uint32_t *)(db->data + db->data_size - 8))  = htonl(db->data_size);
    *((uint16_t *)(db->data + db->data_size - 10)) = htons(PREFIXDB_LIBRARY_VERSION);
    *((uint32_t *)(db->data + db->data_size - 14)) = htonl(db->nodes_count);
    *((uint8_t  *)(db->data + db->data_size - 15)) = (uint8_t)(db->records_size * 8);

    // nodes writing (pass 3)
    pnode = &(db->nodes);
    while (1)
    {
        if (pnode->flaggued != (db->pass + 2) && (pnode->id || pnode == &(db->nodes)))
        {
            pnode->flaggued = (db->pass + 2);
            prefixdb_write_node
            (
                db, pnode->id,
                pnode->down[0] ? (pnode->down[0]->id ? pnode->down[0]->id : db->nodes_count + 16) : db->nodes_count,
                pnode->down[1] ? (pnode->down[1]->id ? pnode->down[1]->id : db->nodes_count + 16) : db->nodes_count
            );
        }
        while (pnode->down[0] && pnode->explored[0] != (db->pass + 3))
        {
            pnode->explored[0] = (db->pass + 3);
            pnode              = pnode->down[0];
            if (pnode->flaggued != (db->pass + 2) && pnode->id)
            {
                pnode->flaggued = (db->pass + 2);
                prefixdb_write_node
                (
                    db, pnode->id,
                    pnode->down[0] ? (pnode->down[0]->id ? pnode->down[0]->id : db->nodes_count + 16) : db->nodes_count,
                    pnode->down[1] ? (pnode->down[1]->id ? pnode->down[1]->id : db->nodes_count + 16) : db->nodes_count
                );
            }
        }
        if (pnode->down[1] && pnode->explored[1] != (db->pass + 3))
        {
            pnode->explored[1] = (db->pass + 3);
            pnode              = pnode->down[1];
            continue;
        }
        if (pnode == &(db->nodes))
        {
            break;
        }
        pnode = pnode->up;
    }

    return PREFIXDB_ERROR_OK;
}

int prefixdb_save_binary(PREFIXDB *_db, uint8_t **data, uint32_t *size, uint8_t flags)
{
    _PREFIXDB *db = (_PREFIXDB *)_db;

    if (!db || prefixdb_serialize(db) != PREFIXDB_ERROR_OK)
    {
        return PREFIXDB_ERROR_PARAM;
    }
    if (data)
    {
        if (flags & PREFIXDB_FLAGS_COPY)
        {
            if (!(*data = (uint8_t *)malloc(db->data_size)))
            {
                return PREFIXDB_ERROR_MEMORY;
            }
            memcpy(*data, db->data, db->data_size);
        }
        else
        {
            *data = db->data;
        }
    }
    if (size) *size = db->data_size;
    return PREFIXDB_ERROR_OK;
}

int prefixdb_save_file(PREFIXDB *_db, const char *path)
{
    FILE     *handle;
    uint32_t size;
    uint8_t  *data;
    int      status;

    if (!path)
    {
        return PREFIXDB_ERROR_PARAM;
    }
    if ((status = prefixdb_save_binary(_db, &data, &size, 0)))
    {
        return status;
    }
    if (!(handle = fopen(path, "w")))
    {
        return PREFIXDB_ERROR_ACCESS;
    }
    if (fwrite(data, 1, size, handle) != size)
    {
        fclose(handle);
        return PREFIXDB_ERROR_ACCESS;
    }
    fclose(handle);
    return PREFIXDB_ERROR_OK;
}

int prefixdb_search_binary(PREFIXDB *_db, uint32_t address, PREFIXDBINFO **_info)
{
    _PREFIXDB *db = (_PREFIXDB *)_db;
    uint32_t  next;
    uint8_t   bit = 31, records_size, *pnode, *precord, count;

    if (!db || prefixdb_serialize(db) != PREFIXDB_ERROR_OK)
    {
        return PREFIXDB_ERROR_PARAM;
    }
    records_size = db->records_size;
    pnode        = db->data;
    do
    {
        precord = pnode + ((address & (1 << bit)) ? records_size : 0);
        next    = 0;
        for (count = 0; count < records_size; count ++)
        {
            next += (*(precord + count) << (records_size - count - 1) * 8);
        }
        if (next == db->nodes_count)
        {
            return PREFIXDB_ERROR_NOTFOUND;
        }
        else if (next > db->nodes_count)
        {
            return PREFIXDB_ERROR_OK;
        }
        pnode = db->data + (next * (records_size * 2));
    } while (bit --);
    return PREFIXDB_ERROR_PARAM;
}

int prefixdb_search_string(PREFIXDB *_db, const char *_address, PREFIXDBINFO **_info)
{
    struct in_addr address;

    if (!_address || !inet_aton(_address, &address))
    {
        return PREFIXDB_ERROR_PARAM;
    }
    return prefixdb_search_binary(_db, htonl(address.s_addr), _info);
}

int prefixdb_free_info(PREFIXDBINFO **_info)
{
    return PREFIXDB_ERROR_OK;
}
