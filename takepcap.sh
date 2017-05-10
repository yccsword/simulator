#!/bin/sh
tcpdump -i eth1 host 192.168.1.130 -w test/discover.pcap
