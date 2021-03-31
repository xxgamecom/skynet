rem bind CPU0, CPU1, CPU2, CPU3
start /affinity 0x0f test_net_stress_server.exe 0.0.0.0 10001 4 16384
