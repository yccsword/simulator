brctl addbr bridgeWTP
brctl addif bridgeWTP eth0

ifconfig eth0 up
ifconfig eth0 0
dhclient bridgeWTP





#brctl addif bridgeWTP WTPWLan00

#ifconfig eth0 up
#ifconfig WTPWLan00 up
#ifconfig eth0 0
#ifconfig WTPWLan00 0
#dhclient bridgeWTP
