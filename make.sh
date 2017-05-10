#!/bin/sh
make -j5
#make
killall -9 WTP
cp WTP test/
cp config.wtp test/
cp settings.wtp.txt test/
cp root.pem test/
cp client.pem test/
cp simulator_config test/
