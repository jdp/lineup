#!/usr/bin/env php -q
<?php
include "lineup.php";

define('TESTS', 1000);

try {
	$lineup = new Lineup("localhost", 9876);
}
catch (Exception $e) {
	die($e->getMessage);
}

$data = file_get_contents("loremipsum.txt");
$size = filesize("loremipsum.txt");
echo sprintf("testing with loremipsum.txt, %d bytes in length\n", $size);

$start_time = microtime(true);
for ($i = 0; $i < TESTS; $i++) {
	$lineup->give($i, $data, $size);
}
$end_time = microtime(true);
echo sprintf("finished %d give operations in %f seconds (~%d ops/sec)\n", TESTS, $end_time-$start_time, TESTS/($end_time-$start_time));

$start_time = microtime(true);
for ($i = 0; $i < TESTS; $i++) {
	$lineup->take($i, $data, $size);
}
$end_time = microtime(true);
echo sprintf("finished %d take operations in %f seconds (~%d ops/sec)\n", TESTS, $end_time-$start_time, TESTS/($end_time-$start_time));
