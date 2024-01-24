#!/bin/bash

print_usage () {
    echo "Usage: $0 (-i ROOT_PID [...]|-r|-f)"
}

if [ "$#" -lt 1 ]; then
	print_usage
	exit 1
fi

if [ "$1" = "-f" ]; then
	# set up ftrace

	# After system restart the second chmod may actually not work,
	# even if run as root. Experiments show that it is probably
	# caused by lazy creation of tracefs, because poking this
	# filesystem in some other way (i.e. ls) magically causes
	# chmod to work again. So, do a dummy ls before second chmod.
	chmod 755 /sys/kernel/debug
	if [ "$?" -ne 0 ]; then exit "$?"; fi
	ls /sys/kernel/debug/tracing 1>/dev/null 2>/dev/null
	chmod 755 /sys/kernel/debug/tracing
	chmod 755 /sys/kernel/debug/tracing/trace_pipe
	chmod 755 -R /sys/kernel/debug/tracing/per_cpu/
	if [ "$?" -ne 0 ]; then exit "$?"; fi

	# set buffers size
	echo "262144" > /sys/kernel/debug/tracing/buffer_size_kb
	if [ "$?" -ne 0 ]; then
		exit "$?"
	fi

        # Remove log headers
        echo "nocontext-info" > /sys/kernel/debug/tracing/trace_options

	# clear buffers
	echo "dummy" > "/sys/kernel/debug/tracing/trace"
	if [ "$?" -ne 0 ]; then
		exit "$?"
	fi

elif [ "$1" = "-i" ]; then
	if [ "$#" -lt 2 ]; then
		print_usage
		exit 1
	fi

	# install module
    echo "Trying to support wsl"
    SUPPORT_NS_PID=""
	uname -r | grep WSL > /dev/null && SUPPORT_NS_PID="support_ns_pid=1"
    echo SUPPORT_NS_PID=$SUPPORT_NS_PID
	/sbin/modprobe bas_tracer root_pid="$2" "$SUPPORT_NS_PID" "${@:3}"
	exit "$?"

elif [ "$1" = "-r" ]; then
	/sbin/rmmod bas_tracer
	exit "$?"
else
	print_usage
fi

