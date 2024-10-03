#!/bin/bash

tests=$(ls | grep 'test_.*cc$')
for test in $tests;
do
    make run_test testfile="$test" > /dev/null
    ./"$test".binary > "$test".out
    diff "$test".expected "$test".out
    ret=$?
    if [[ $ret -ne 0 ]]; then
        echo "$test" FAILED
    else 
        echo "$test" passed
    fi
done
