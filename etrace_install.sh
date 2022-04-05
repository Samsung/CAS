#!/bin/bash

cp -f setup_etrace.sh /usr/bin/setup_etrace.sh
chown root:root /usr/bin/setup_etrace.sh
chmod 755 /usr/bin/setup_etrace.sh
cp -f etrace_sudoers /etc/sudoers.d/etrace
