# This file is part of the PrefixDB library
# Copyright (c) 2014 Pierre-Yves Kerembellec <py.kerembellec@gmail.com>

PHP_ARG_ENABLE(prefixdb, whether to enable PrefixDB support,
[  --enable-prefixdb       enable PrefixDB support])

if test "$PHP_PREFIXDB" = "yes"; then
  AC_DEFINE(HAVE_PREFIXDB, 1, [whether you have PrefixDB support])
  PHP_ADD_LIBRARY_WITH_PATH(prefixdb, /usr/lib, PREFIXDB_SHARED_LIBADD)
  PHP_NEW_EXTENSION(prefixdb, prefixdb.c, $ext_shared, , "-Wall")
  PHP_SUBST(PREFIXDB_SHARED_LIBADD)
fi
