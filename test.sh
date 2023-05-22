#/bin/bash
counter=0
while true
do
    make clean > /dev/null
    make run_server > /dev/null
    sleep 1
    OUTPUT=$(sort lettori.log | md5sum | cut -d ' ' -f 1)
    EXPECTED_SUM=$(md5sum expected_out.txt | cut -d ' ' -f 1)

    echo "$OUTPUT"
    echo "$EXPECTED_SUM"

    if [ "$OUTPUT" != "$EXPECTED_SUM" ]
    then
        echo "TEST FAILED"
        cat lettori.log
        echo "========="
        cat expected_out.txt
        echo "WORKED: $counter"
        break
        #PID_PY=$(pgrep -f server.py)
        #kill -9 "$PID_PY"

        #PID_PY=$(pgrep -f archivio)
        #kill -9 "$PID_PY"
    else
        counter=$((counter + 1))

        echo "PASSED $counter"
        
    fi
    echo "------"
    
done