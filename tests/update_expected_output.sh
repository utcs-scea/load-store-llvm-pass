tests=$(ls | grep 'test_.*cc$')
for test in $tests;
do
    make run_test testfile="$test" > /dev/null
    ./"$test".binary > "$test".expected
done
for test in $tests;
do
    make run_test_o0 testfile="$test" > /dev/null
    ./"$test".binary > "$test".expected_o0
done