#!/bin/bash
UNQ=../unq/bin/Release/unq
JSONCOMPARE=../JSONCompare/bin/Release/JSONCompare
EMPLOYEES=../tutorial-samples/employees

function test_query() {
    basefile=$2${1##*/}
    $UNQ -f $1 ${@:3} >results/$basefile 2> results/${basefile}.errors
    if [ -s expected/$basefile ]; then
	$JSONCOMPARE expected/$basefile results/$basefile
    elif [ -s results/$basefile ]; then
	echo Query $f, expected not found.
    fi
    diff -Naur expected/$basefile.errors results/$basefile.errors || true
}


rm -rf results
mkdir results
for f in ${EMPLOYEES}/queries/*.unq; do
    test_query $f employee_ $EMPLOYEES/employee*.json
done

for f in parsing/*.unq; do
    test_query $f parsing_ $EMPLOYEES/employee1.json
done

for f in stackoverflow/*.unq; do
    test_query $f stackoverflow_ ${f%.*}.json
done
