#!/bin/sh
./fill_database clearap
./fill_database clearsta
./fill_database clearradio
./fill_database addfile radio_productinfo-80000-0
./fill_database addfile sta_productinfo-80000-0
./fill_database addfile ap_productinfo-40000-1
