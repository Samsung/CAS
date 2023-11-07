#!/bin/bash

cp -f setup_etrace.sh /usr/bin/setup_etrace.sh
chown root:root /usr/bin/setup_etrace.sh
chmod 755 /usr/bin/setup_etrace.sh
if [ -d /etc/sudoers.d/ ]; then
    cp -f etrace_sudoers /etc/sudoers.d/etrace
    chmod 440 /etc/sudoers.d/etrace
fi

echo "The following entry has been added to the '/etc/sudoers.d/etrace' file"
echo "  ALL ALL = (root) NOPASSWD: /usr/bin/setup_etrace.sh"
echo "This should allow to run the etrace command for any user without sudo password"
echo "In case of problems please make sure the following line is located at the end of the '/etc/sudoers' file"
echo "  #includedir /etc/sudoers.d"
echo