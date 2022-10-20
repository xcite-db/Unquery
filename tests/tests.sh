#!/bin/bash
UNQ=../unq/bin/Release/unq
JSONCOMPARE=../JSONCompare/bin/Release/JSONCompare
EMPLOYEES=../tutorial-samples/employees

rm -rf results
mkdir results
for f in ${EMPLOYEES}/queries/*.unq; do
    basefile=employee_${f##*/}
    $UNQ -f $f ${EMPLOYEES}/employee*.json >results/$basefile 2> results/${basefile}.errors
    $JSONCOMPARE expected/$basefile results/$basefile
    diff -Naur expected/$basefile.errors results/$basefile.errors
done
