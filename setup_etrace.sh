#!/bin/bash

# following will be set to all cpu during tracing 
SAFE_BUFFER_SIZE="262144" 

# following will overwrite safe buffer with TRACING_BUFF_SIZE_KB env
# in case of running etrace -c %RANGE% will be applied only to %RANGE%
BUFFER_SIZE="${TRACING_BUFF_SIZE_KB:-262144}" 

# following will be set to all cpu after tracing
DEFAULT_BUFFER_SIZE="1410"

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
	if [ "$2" == "-c" ]; then
		CORE_RANGE=$(seq ${3/-/' '})
		# reset buffers size
		echo "${SAFE_BUFFER_SIZE}" > /sys/kernel/debug/tracing/buffer_size_kb
		for cpu_buff_file in /sys/kernel/debug/tracing/per_cpu/cpu*/buffer_size_kb ; do 
			echo "${SAFE_BUFFER_SIZE}" > "$cpu_buff_file"; 
		done
		# per-cpu setup
		for i in $CORE_RANGE ; do
			echo ${BUFFER_SIZE} > "/sys/kernel/debug/tracing/per_cpu/cpu${i}/buffer_size_kb";
		done
	else 
		echo "${BUFFER_SIZE}" > /sys/kernel/debug/tracing/buffer_size_kb
	fi
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
    SUPPORT_NS_PID=""
	uname -r | grep WSL > /dev/null && SUPPORT_NS_PID="support_ns_pid=1"
	if [ ! -z "${SUPPORT_NS_PID}" ]; then
		echo SUPPORT_NS_PID=$SUPPORT_NS_PID
	fi
	/sbin/modprobe bas_tracer root_pid="$2" "$SUPPORT_NS_PID" "${@:3}"
	exit "$?"

elif [ "$1" = "-r" ]; then
	/sbin/rmmod bas_tracer
	echo "${DEFAULT_BUFFER_SIZE}" > /sys/kernel/debug/tracing/buffer_size_kb
	exit "$?"
else
	print_usage
fi

