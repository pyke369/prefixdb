// This file is part of the PrefixDB library
// Copyright (c) 2014 Pierre-Yves Kerembellec <py.kerembellec@gmail.com>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "php.h"

#include <libprefixdb.h>

static HashTable prefixdb_cache;

typedef struct
{
    PREFIXDB *db;
    time_t   checked, modified;
} PREFIXDB_CACHE;

typedef struct
{
    zend_object zo;
    PREFIXDB    *db;
    int         cached;
} PREFIXDB_OBJECT;

zend_class_entry *prefixdb_class_entry;

PHP_METHOD(PrefixDB, __construct)
{
    PREFIXDB_OBJECT *instance = (PREFIXDB_OBJECT *)zend_object_store_get_object(getThis() TSRMLS_CC);
    PREFIXDB_CACHE  *cache, **pcache;
    PREFIXDB        *pfdb;
    struct stat     info;
    time_t          now;
    char            *path = NULL;
    int             length = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &path, &length) == SUCCESS && length)
    {
        if (zend_hash_find(&prefixdb_cache, path, length + 1, (void **)&pcache) == SUCCESS)
        {
            time(&now);
            if (now - (*pcache)->checked >= 30)
            {
                (*pcache)->checked = now;
                if (!stat(path, &info) && info.st_mtime > (*pcache)->modified && info.st_mtime <= (now - 5))
                {
                    if ((pfdb = prefixdb_load_file(path, 0)))
                    {
                        prefixdb_free((*pcache)->db);
                        (*pcache)->db       = pfdb;
                        (*pcache)->modified = info.st_mtime;
                    }
                    else
                    {
                        php_error_docref(NULL TSRMLS_CC, E_WARNING, "cannot open or invalid PrefixDB database \"%s\"", path);
                    }
                }
            }
            instance->db     = (*pcache)->db;
            instance->cached = 1;
        }
        else
        {
            if (!(instance->db = prefixdb_load_file(path, 0)))
            {
                php_error_docref(NULL TSRMLS_CC, E_WARNING, "cannot open or invalid PrefixDB database \"%s\"", path);
            }
            else if ((cache = calloc(1, sizeof(PREFIXDB_CACHE))))
            {
                time(&cache->checked);
                cache->modified  = cache->checked;
                cache->db        = instance->db;
                instance->cached = 1;
                if (!stat(path, &info))
                {
                    cache->modified = info.st_mtime;
                }
                zend_hash_add(&prefixdb_cache, path, length + 1, &cache, sizeof(PREFIXDB_CACHE *), NULL);
            }
        }
    }
    else if (!instance->db)
    {
        instance->db = prefixdb_allocate();
    }
}

PHP_METHOD(PrefixDB, add)
{
    PREFIXDB_OBJECT *instance = (PREFIXDB_OBJECT *)zend_object_store_get_object(getThis() TSRMLS_CC);
    char            *prefix   = NULL;
    int             length    = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &prefix, &length) == SUCCESS && length && !instance->cached)
    {
        RETURN_LONG(prefixdb_add_string(instance->db, prefix, NULL));
    }
    RETURN_LONG(PREFIXDB_ERROR_PARAM);
}

PHP_METHOD(PrefixDB, save)
{
    PREFIXDB_OBJECT *instance = (PREFIXDB_OBJECT *)zend_object_store_get_object(getThis() TSRMLS_CC);
    char            *path     = NULL;
    int             length    = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &path, &length) == SUCCESS && length)
    {
        RETURN_LONG(prefixdb_save_file(instance->db, path));
    }
    RETURN_LONG(PREFIXDB_ERROR_PARAM);
}

PHP_METHOD(PrefixDB, search)
{
    PREFIXDB_OBJECT *instance = (PREFIXDB_OBJECT *)zend_object_store_get_object(getThis() TSRMLS_CC);
    char            *address  = NULL;
    int             length    = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &address, &length) == SUCCESS && length)
    {
        if (prefixdb_search_string(instance->db, address, NULL) == PREFIXDB_ERROR_OK)
        {
            RETURN_STRING("{}", 1);
        }
    }
    RETURN_NULL();
}

static void prefixdb_dtor(PREFIXDB_OBJECT *instance TSRMLS_DC)
{
    zend_object_std_dtor(&(instance->zo) TSRMLS_CC);
    if (!instance->cached)
    {
        prefixdb_free(&(instance->db));
    }
    efree(instance);
    instance = NULL;
}

static zend_object_value prefixdb_ctor(zend_class_entry *class_entry TSRMLS_DC)
{
    zend_object_value value;
    PREFIXDB_OBJECT   *instance;

    instance = ecalloc(1, sizeof(PREFIXDB_OBJECT));
    zend_object_std_init(&(instance->zo), class_entry TSRMLS_CC);
    value.handle = zend_objects_store_put(instance, (zend_objects_store_dtor_t)zend_objects_destroy_object,
                                                    (zend_objects_free_object_storage_t)prefixdb_dtor, NULL TSRMLS_CC);
    value.handlers = zend_get_std_object_handlers();
    return value;
}

static zend_function_entry prefixdb_class_methods[] =
{
    PHP_ME(PrefixDB, __construct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(PrefixDB, add,         NULL, ZEND_ACC_PUBLIC)
    PHP_ME(PrefixDB, save,        NULL, ZEND_ACC_PUBLIC)
    PHP_ME(PrefixDB, search,      NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

PHP_MINIT_FUNCTION(prefixdb)
{
    zend_class_entry class_entry;
    INIT_CLASS_ENTRY(class_entry, "PrefixDB", prefixdb_class_methods);
    prefixdb_class_entry = zend_register_internal_class(&class_entry TSRMLS_CC);
    prefixdb_class_entry->create_object = prefixdb_ctor;
    zend_hash_init(&prefixdb_cache, 256, NULL, NULL, 1);
    REGISTER_LONG_CONSTANT("PREFIXDB_ERROR_OK",       PREFIXDB_ERROR_OK,       CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PREFIXDB_ERROR_PARAM",    PREFIXDB_ERROR_PARAM,    CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PREFIXDB_ERROR_MEMORY",   PREFIXDB_ERROR_MEMORY,   CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PREFIXDB_ERROR_ACCESS",   PREFIXDB_ERROR_ACCESS,   CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PREFIXDB_ERROR_NOTFOUND", PREFIXDB_ERROR_NOTFOUND, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PREFIXDB_FLAGS_COPY",     PREFIXDB_FLAGS_COPY,     CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PREFIXDB_FLAGS_MMAP",     PREFIXDB_FLAGS_MMAP,     CONST_CS | CONST_PERSISTENT);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(prefixdb)
{
    PREFIXDB_CACHE **cache;
    HashPosition   position;

    for (zend_hash_internal_pointer_reset_ex(&prefixdb_cache, &position);
         zend_hash_get_current_data_ex(&prefixdb_cache, (void **)&cache, &position) == SUCCESS;
         zend_hash_move_forward_ex(&prefixdb_cache, &position))
    {
        prefixdb_free((*cache)->db);
        free(*cache);
    }
    zend_hash_destroy(&prefixdb_cache);
    return SUCCESS;
}

zend_module_entry prefixdb_module_entry =
{
    STANDARD_MODULE_HEADER,
    "prefixdb",
    NULL,
    PHP_MINIT(prefixdb),
    PHP_MSHUTDOWN(prefixdb),
    NULL,
    NULL,
    NULL,
    "1.1",
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PREFIXDB
ZEND_GET_MODULE(prefixdb)
#endif
