#!/bin/sh
apcount=$1
radiocount=$[apcount*2]
stacount=$[apcount*20]
#echo $apcount
#echo $radiocount
#echo $stacount
./fill_database clearap
./fill_database clearsta
./fill_database clearradio
./fill_database addradio radio_productinfo $radiocount
./fill_database addsta sta_productinfo $stacount
./fill_database addap ap_productinfo $apcount 20
