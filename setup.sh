#!/bin/bash

sudo apt-get install autoconf automake libglib2.0-dev libtool libpcre3-dev flex bison dkms libfdt-dev libbsd-dev

autoreconf -ivf
./configure
nake
sudo make install

echo "=========="
echo "Running test: fwts uefirtvariable"
fwts uefirtvariable
