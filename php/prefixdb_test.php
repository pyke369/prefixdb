#!/usr/bin/env php
<?php
// This file is part of the PrefixDB library
// Copyright (c) 2014 Pierre-Yves Kerembellec <py.kerembellec@gmail.com>

define('PREFIXES_COUNT', 500000);
define('SEARCHES_COUNT', 500000);

$exit  = 0;
$begin = microtime(true); $pfdb = new PrefixDB(); $end = microtime(true);
print sprintf("allocate empty database   pass [%.06fs]\n", $end - $begin);

print sprintf("add %6d prefixes       ", PREFIXES_COUNT); fflush(STDOUT);
$status = 0;
$begin  = microtime(true);
for ($count = 0; $count < PREFIXES_COUNT; $count ++)
{
    $status |= $pfdb->add(sprintf('%d.%d.%d.%d/%d', rand(1, 223), rand(1, 223), rand(1, 223), rand(1, 223), rand(16, 28)));
    if ($count && !($count % 25000))
    {
        print '.'; fflush(STDOUT);
    }
}
$end = microtime(true);
print sprintf("\radd %6d prefixes       %s [%.06fs] [%d prefixes/s]", PREFIXES_COUNT, $status ? 'fail' : 'pass', $end - $begin, PREFIXES_COUNT / ($end - $begin));
for ($count = 0; $count < PREFIXES_COUNT / 25000; $count ++) print ' '; print "\n";
$exit |= ($status ? 1 : 0);

$begin = microtime(true); $status = $pfdb->save('/tmp/bench.pfdb'); $end = microtime(true);
print sprintf("save database             %s [%.06fs]\n", $status == PREFIXDB_ERROR_OK ? 'pass' : 'fail', $end - $begin);
$exit |= ($status != PREFIXDB_ERROR_OK ? 1 : 0);

$begin = microtime(true); $pfdb = new PrefixDB('/tmp/bench.pfdb'); $end = microtime(true);
printf("load database             pass [%.06fs]\n", $end - $begin);

print sprintf("search %6d addresses   ", SEARCHES_COUNT); fflush(STDOUT);
$matches = array(0, 0);
$begin   = microtime(true);
for ($count = 0; $count < SEARCHES_COUNT; $count ++)
{
    $status = $pfdb->search(sprintf('%d.%d.%d.%d', rand(1, 254), rand(1, 254), rand(1, 254), rand(1, 254)));
    if      ($status === null) $matches[1] ++;
    else                       $matches[0] ++;
    if ($count && !($count % 25000))
    {
        printf('.'); fflush(STDOUT);
    }
}
$end = microtime(true);
print sprintf("\rsearch %6d addresses   pass [%.06fs] [%d searches/s - %d matched - %d unmatched]",
              SEARCHES_COUNT, $end - $begin, SEARCHES_COUNT / ($end - $begin), $matches[0], $matches[1]);
for ($count = 0; $count < SEARCHES_COUNT / 25000; $count ++) print ' '; print "\n";

unlink('/tmp/bench.pfdb');
exit($exit);
