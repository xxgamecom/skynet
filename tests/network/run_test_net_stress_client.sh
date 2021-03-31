rem bind CPU4, CPU5, CPU6, CPU7
start /affinity 0xf0 test_net_stress_client.exe 127.0.0.1 10001 4 100 16384 100
