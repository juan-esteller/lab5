#!/bin/bash 
function compare_results {
    CACHE=$1
    TEST=`basename $2 .test`
    ./main $CACHE "tests/$TEST.test" > result.txt 

    if diff result.txt "tests/results_$CACHE/$TEST.txt"; 
    then
        echo "Passed test $TEST with cache $CACHE"
    else 
        echo "Failed test $TEST with cache $CACHE"
    fi
    rm result.txt
}

for t in $(ls tests | grep test)
do
    compare_results $1 $t
done 
