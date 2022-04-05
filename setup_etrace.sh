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

	NPROC=$(nproc)
	NPROC=$(expr $NPROC - 1)

	# set buffers size
	echo "131072" > /sys/kernel/debug/tracing/buffer_size_kb
	if [ "$?" -ne 0 ]; then
		exit "$?"
	fi

        # Remove log headers
        echo "nocontext-info" > /sys/kernel/debug/tracing/trace_options

	# clear buffers
	for i in `seq 0 $NPROC`; do
		echo "dummy" > "/sys/kernel/debug/tracing/per_cpu/cpu$i/trace"
		if [ "$?" -ne 0 ]; then
			exit "$?"
		fi
	done

elif [ "$1" = "-i" ]; then
	if [ "$#" -lt 2 ]; then
		print_usage
		exit 1
	fi

	# install module
	/sbin/modprobe execve_trace root_pid="$2" "${@:3}"
	exit "$?"

elif [ "$1" = "-r" ]; then
	/sbin/rmmod execve_trace
	exit "$?"
else
	print_usage
fi

