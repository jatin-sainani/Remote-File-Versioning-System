#!/bin/bash

echo "Test 1: WRITE"
echo "hello world" > t1.txt
./rfs WRITE t1.txt foo/bar.txt

echo "Test 2: GET"
./rfs GET foo/bar.txt out1.txt

echo "Test 3: WRITE overwrite"
echo "new version" > t2.txt
./rfs WRITE t2.txt foo/bar.txt


./rfs GET foo/bar.txt /dev/null

echo "Test 4: LS"
./rfs LS foo/bar.txt

echo "Test 5: GET version 1"
./rfs GET foo/bar.txt out_v1.txt 1

echo "Test 6: RM"
./rfs RM foo/bar.txt
