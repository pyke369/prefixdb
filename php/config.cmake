project(prefixdb)

find_library(libprefixdb REQUIRED)
set(LIBPREFIXDB_LIBRARIES libprefixdb.so)

link_directories('/usr/lib')

HHVM_COMPAT_EXTENSION(hhlibprefixdb prefixdb.c)
HHVM_SYSTEMLIB(hhlibprefixdb ext_prefixdb.php)

target_link_libraries(hhlibprefixdb ${LIBPREFIXDB_LIBRARIES})
