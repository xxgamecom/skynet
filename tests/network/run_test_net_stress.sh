#!/bin/sh

killall -9 test_net_stress_client
killall -9 test_net_stress_server

timeout=${timeout:-100}
bufsize=${bufsize:-16384}

for nosessions in 100 1000; do
  for nothreads in 1 2 3 4; do
    sleep 5
    echo "Bufsize: $bufsize Threads: $nothreads Sessions: $nosessions"
    ./test_net_stress_server 0.0.0.0 10001 $nothreads $bufsize & srvpid=$!
    sleep 1
    ./test_net_stress_client 127.0.0.1 10001 $nothreads $nosessions $bufsize $timeout
    kill -9 $srvpid
  done
done
