#!/bin/bash

tests=$(ls | grep 'test_.*cc$')
for test in $tests;
do
    make run_test testfile="$test" > /dev/null
    ./"$test".binary > "$test".out
    diff "$test".expected "$test".out > /dev/null
    ret=$?
    if [[ $ret -ne 0 ]]; then
        echo "$test" \(O3\) FAILED
    else 
        echo "$test" \(O3\) passed
    fi
done
