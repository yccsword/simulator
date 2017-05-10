iw WTPWLan00 del
iw monitor0 del
ip link set bridge0 down
brctl delbr bridge0
kill -9 `pidof WTP`
make WTP
./WTP .
#valgrind --verbose --leak-check=full --show-reachable=yes --log-file="ValgrindWTP.txt" ./WTP . &

