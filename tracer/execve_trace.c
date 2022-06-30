#define pr_fmt(fmt) "exec_trace: " fmt

#include <asm/syscall.h>
#include <linux/binfmts.h>
#include <linux/dcache.h>
#include <linux/fdtable.h>
#include <linux/fs_struct.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/trace_events.h>
#include <linux/tracepoint.h>
#include <linux/uaccess.h>
#include <linux/version.h>

MODULE_DESCRIPTION("Module that tracks multiple syscalls via tracepoints infrastructure");
MODULE_AUTHOR("SRPOL.MB.SEC");
MODULE_LICENSE("GPL");

// _THIS_IP_ is used internally by trace_printk() macro to show the address of
// code that the given trace log comes from. We don't need that, so to make the
// log a bit smaller (and keep compatibility with older solution based on
// __trace_printk(0,...)) set _THIS_IP_ to 0
#ifdef _THIS_IP_
#undef _THIS_IP_
#define _THIS_IP_ 0LL
#endif

#define XSTR(s) STR(s)
#define STR(s) #s
#define BITMASK(n) (~(((u64)-1) << n))

static int bits_of_real_pid; // defined later

// A unique process identifier that should not repeat over a single build time.
// The MSB is cleared to avoid potential problems with databases which poorly
// support 64-bit unsigned integers.
// Assuming the PID doesn't exceed 2^22 (4 million, see PID_MAX_LIMIT in
// threads.h).
#define UNIQUE_PID_FOR_TASK(upid, tsk) \
	((s64)((upid << bits_of_real_pid & 0x7fffffffffffffff) \
	 | (tsk->pid & BITMASK(bits_of_real_pid))))
//#define UNIQUE_PID (UNIQUE_PID_FOR_TASK(current))

#define INVALID_UPID ((u64)-1)
#define MAX_STR_LEN_PER_LINE 900
#define MAX_REP_OPEN_PATH_LEN_EXP 10
#define MAX_REP_OPEN_PATH_LEN (1 << MAX_REP_OPEN_PATH_LEN_EXP)
#define MAX_REP_OPEN_FDS 16

// Based on the original 'trace_printk' and 'do_trace_printk' macros from
// linux/kernel.h.
// __trace_bprintk() doesn't work well with long strings (>1024 chars) - it
// looks like it aborts if the string cannot fully fit into binary buffer, even
// if there is a precision specifier in the format string. __trace_printk()
// seems to behave properly, but for some reason it requires __trace_printk_fmt
// section to be present in the module - otherwise it panics the kernel with
// a stack overflow error.
// This version of the 'trace_printk' macros defines correct variables in
// __trace_printk_fmt section as original, but always calls __trace_printk().
// Perhaps there is a performance penalty from using __trace_printk() where
// __trace_bprintk() would be used, but this should be safer than other
// solutions.
#define my_trace_printk(fmt, args...)					\
do {									\
	char _______STR[] = __stringify((args));			\
	if (sizeof(_______STR) <= 3) {					\
		trace_puts(fmt);					\
	} else {							\
		static const char *trace_printk_fmt __used		\
				__section("__trace_printk_fmt") =	\
				__builtin_constant_p(fmt) ? fmt : NULL;	\
									\
		__trace_printk_check_format(fmt, ##args);		\
									\
		__trace_printk(_THIS_IP_, fmt, ##args);			\
	}								\
} while (0)								\

#if LINUX_VERSION_CODE > KERNEL_VERSION(5,8,0)
// if called directly, fmt should always come from SNPRINTF_TRACE
#define PRINT_TRACE_TASK_RAW(upid, tsk, fmt, ...) \
	do {  \
		struct timespec64 ts;  \
		ktime_get_ts64(&ts);  \
		my_trace_printk(fmt,  \
			UNIQUE_PID_FOR_TASK(upid, tsk), smp_processor_id(), \
			ts.tv_sec, ts.tv_nsec, ##__VA_ARGS__);  \
	} while (0)
#define PRINT_TRACE_TASK(upid, tsk, fmt, ...) \
	PRINT_TRACE_TASK_RAW(upid, tsk, "%lld,%d,%lld,%ld" fmt "\n", ##__VA_ARGS__)
#else
// if called directly, fmt should always come from SNPRINTF_TRACE
#define PRINT_TRACE_TASK_RAW(upid, tsk, fmt, ...) \
	do {  \
		struct timespec ts;  \
		ktime_get_ts(&ts);  \
		my_trace_printk(fmt,  \
			UNIQUE_PID_FOR_TASK(upid, tsk), smp_processor_id(), \
			ts.tv_sec, ts.tv_nsec, ##__VA_ARGS__);  \
	} while (0)
#define PRINT_TRACE_TASK(upid, tsk, fmt, ...) \
	PRINT_TRACE_TASK_RAW(upid, tsk, "%lld,%d,%ld,%ld" fmt "\n", ##__VA_ARGS__)
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(5,8,0) */

#define PRINT_TRACE(upid, fmt, ...) \
	PRINT_TRACE_TASK(upid, current, fmt, ##__VA_ARGS__)

#define SNPRINTF_TRACE(buf, size, fmt, ...) \
	snprintf(buf, size, "%%lld,%%d,%%ld,%%ld" fmt "\n", ##__VA_ARGS__)


// module's params

static int root_pid = 0;
module_param(root_pid, int, 0 /* don't expose it to sysfs */);
static int ignore_repeated_opens = 0;
module_param(ignore_repeated_opens, int, 0);

// amount of bits occupied by process' real PID in unique PID value.
// If set to zero, the unique PID will consist only of our own upid.
// Acceptable values are from range [0, 32].
static int bits_of_real_pid = 22;
module_param(bits_of_real_pid, int, 0);

// private global variables

static DEFINE_MUTEX(list_mutex);

// access/change must be protected by list_mutex
static u64 next_upid = 1;

struct last_open_t {
	// only the last MAX_REP_OPEN_PATH_LEN bytes of path is compared
	char fname[MAX_REP_OPEN_PATH_LEN];
	int flags;
	int mode;
	// the list of fds related to this (filename,mode,flags) tuple (for
	// cases when a single file is opened multiple times at once). Add an fd
	// to this list when there is an open() of this tuple with a new fd and
	// remove an fd from this list when that fd is closed. This way this
	// list should always contain all opened fds that refer to that file.
	// The first (fds[0]) element on this list is special in that it
	// contains the fd returned by the first open call that referred to this
	// last_open_t entry (the one that created it). We distinquish it from
	// the rest so that we can print only a corresponding close event with
	// that same fd that was present in open event.
	int fds[MAX_REP_OPEN_FDS];
	unsigned fds_count;
	bool close_printed;
};

struct traced_pid {
	u64 upid;
	pid_t pid;
	struct last_open_t last_open;
	struct list_head list;
};

static LIST_HEAD(traced_pids);

static struct tracepoint *tracepoint_sched_process_fork = NULL;
static struct tracepoint *tracepoint_sched_process_exit = NULL;
static struct tracepoint *tracepoint_sched_process_exec = NULL;
static struct tracepoint *tracepoint_sys_enter = NULL;
static struct tracepoint *tracepoint_sys_exit = NULL;


// ########## PIDS LIST MANAGEMENT ##########

static bool is_pid_on_list_locked(pid_t pid, u64* upid)
{
	struct traced_pid *tmp = NULL;
	list_for_each_entry(tmp, &traced_pids, list) {
		if (tmp->pid == pid) {
			if (upid)
				*upid = tmp->upid;
			return true;
		}
	}
	return false;
}

// If true is returned, traced_upid will be set (if it is not null) to upid of
// current process.
// If false is returned, traced_upid is untouched.
static bool should_trace(u64* traced_upid)
{
	bool retval = false;
	u64 ret_traced_upid = INVALID_UPID;

	mutex_lock(&list_mutex);
	retval = is_pid_on_list_locked(current->pid, &ret_traced_upid);
	mutex_unlock(&list_mutex);

	if (retval && traced_upid)
		*traced_upid = ret_traced_upid;

	return retval;
}

static u64 maybe_add_pid_to_list_locked(pid_t pid, pid_t ppid, bool force)
{
	struct traced_pid *new_pid;
	bool should_add = false;

	// force == true should only be used to add the root pid
	if (force)
		should_add = true;
	else
		// check if parent pid is tracked
		should_add = is_pid_on_list_locked(ppid, NULL);

	if (!should_add)
		return INVALID_UPID;

	pr_debug("Adding pid %d to list\n", (int) pid);
	new_pid = kzalloc(sizeof(struct traced_pid), GFP_KERNEL);
	if (!new_pid) {
		pr_err("Failed to allocate struct traced_pid\n");
		return INVALID_UPID;
	}
	// TODO: do we need to check if pid already exists on a list?

	new_pid->pid = pid;
	new_pid->upid = next_upid++;
	INIT_LIST_HEAD(&new_pid->list);
	list_add(&new_pid->list, &traced_pids);
	return new_pid->upid;
}

static void remove_pid_from_list_locked(pid_t pid)
{
	struct traced_pid *el, *tmp;
	list_for_each_entry_safe(el, tmp, &traced_pids, list) {
		if (el->pid == pid) {
			pr_debug("Removing pid %d from list\n", (int) pid);
			list_del(&el->list);
			kfree(el);
			return;
		}
	}
}

static void clear_pids_list_locked(void)
{
	struct traced_pid *el, *tmp;
	pr_info("Clearing pids list\n");
	list_for_each_entry_safe(el, tmp, &traced_pids, list) {
		list_del(&el->list);
		kfree(el);
	}
}

static u64 maybe_add_pid_to_list(pid_t pid, pid_t ppid, bool force)
{
	u64 upid = INVALID_UPID;
	mutex_lock(&list_mutex);
	upid = maybe_add_pid_to_list_locked(pid, ppid, force);
	mutex_unlock(&list_mutex);
	return upid;
}

static void remove_pid_from_list(pid_t pid)
{
	mutex_lock(&list_mutex);
	remove_pid_from_list_locked(pid);
	mutex_unlock(&list_mutex);
}

static void clear_pids_list(void)
{
	mutex_lock(&list_mutex);
	clear_pids_list_locked();
	mutex_unlock(&list_mutex);
}


// ########## IGNORING REPEATING OPENS ##########

static bool is_repeating_open(char* path, unsigned long path_size, int flags,
			      int mode, int fd)
{
	struct traced_pid *tmp = NULL;
	bool is_repeating = false;

	mutex_lock(&list_mutex);
	list_for_each_entry(tmp, &traced_pids, list) {
		if (tmp->pid != current->pid)
			continue;

		// tmp->pid == current->pid
		if (tmp->last_open.mode == mode
		    && tmp->last_open.flags == flags) {
			unsigned long str_offset = 0;

			if (path_size >= MAX_REP_OPEN_PATH_LEN) {
				str_offset = path_size - (MAX_REP_OPEN_PATH_LEN - 1);
			}

			if (!strncmp(tmp->last_open.fname,
					path + str_offset,
					MAX_REP_OPEN_PATH_LEN - 1)) {
				int iter, old_fds_count;
				is_repeating = true;

				// add fd to the list of fds for this open.
				// This fd shouldn't be already on the list,
				// because !Close event should have removed it
				// if it was used before

				if (tmp->last_open.fds_count >= MAX_REP_OPEN_FDS) {
					pr_warn("Exceeded number of stored fds in last_open_t!");
					break;
				}

				// since we don't put an fd in the first array
				// slot (see below and struct last_open_t docs),
				// there may be a case where fds_count is less
				// than MAX_REP_OPEN_FDS (more precisely: equal
				// to MAX_REP_OPEN_FDS-1) and we still wouldn't
				// put a new fd to the array. This can happen
				// e.g. when a file is opened MAX_REP_OPEN_FDS
				// times, then the first fd is closed, and then
				// another open() of this file happens.
				// We treat it basically as if there was no more
				// space in fds array.
				if (tmp->last_open.fds_count == MAX_REP_OPEN_FDS - 1
				    && tmp->last_open.fds[0] == -1) {
					pr_debug("Hit [open()*MAX -> close(fd[0]) -> open()] edge case");
					break;
				}

				old_fds_count = tmp->last_open.fds_count;
				// put the fd in the first empty slot, starting from second array entry
				// (first entry is special, see struct last_open_t docs)
				for (iter = 1; iter < MAX_REP_OPEN_FDS; iter++) {
					if (tmp->last_open.fds[iter] == -1) {
						tmp->last_open.fds[iter] = fd;
						tmp->last_open.fds_count++;
						pr_debug("adding fd %d, fds_count = %d",
							 fd,
							 tmp->last_open.fds_count);
						break;
					}
				}
				if (old_fds_count == tmp->last_open.fds_count) {
					pr_err("Bug!: didn't insert fd on list with fds_count < MAX");
				}
			}
		}
		// TODO should use goto instead? (here and in other functions in
		// this seciton)
		break;
	}
	mutex_unlock(&list_mutex);
	return is_repeating;
}

static void update_last_open(char* path, unsigned long path_size, int flags,
			     int mode, int fd)
{
	struct traced_pid *tmp = NULL;
	mutex_lock(&list_mutex);
	list_for_each_entry(tmp, &traced_pids, list) {
		if (tmp->pid == current->pid) {
			unsigned long str_offset = 0;
			int iter;

			// only copy the last MAX_REP_OPEN_PATH_LEN-1 chars
			if (path_size >= MAX_REP_OPEN_PATH_LEN) {
				str_offset = path_size - (MAX_REP_OPEN_PATH_LEN - 1);
			}

			strscpy(tmp->last_open.fname, path + str_offset,
			       MAX_REP_OPEN_PATH_LEN);
			tmp->last_open.flags = flags;
			tmp->last_open.mode = mode;
			tmp->last_open.fds[0] = fd;
			for (iter = 1; iter < MAX_REP_OPEN_FDS; iter++)
				tmp->last_open.fds[iter] = -1;
			tmp->last_open.fds_count = 1;
			tmp->last_open.close_printed = false;
			break;
		}
	}
	mutex_unlock(&list_mutex);
}

/*
 * Updates the last_open cache with regards to !Close event on given fd.
 * Returns true if !Close event for this file was already printed, false otherwise
 */
static bool update_last_open_fd_on_close(int fd)
{
	struct traced_pid *tmp = NULL;
	bool close_printed = false;

	mutex_lock(&list_mutex);
	list_for_each_entry(tmp, &traced_pids, list) {
		int idx = 0;
		bool fd_found = false;

		if (tmp->pid != current->pid)
			continue;

		// tmp->pid == current->pid

		if (!tmp->last_open.fds_count) {
			break; // close_printed = false
		}

		for (; idx < MAX_REP_OPEN_FDS; idx++) {
			if (tmp->last_open.fds[idx] != fd)
				continue;

			// tmp->last_open.fds[idx] == fd

			// Allow potentially printing close event only for the
			// first entry in array (i.e. only the "original" fd
			// from the first open() call may have corresponding
			// close event). This is to maintain consistence between
			// fds printed in open and close events.
			if (!idx) {
				close_printed = tmp->last_open.close_printed;

				// We assume that if this fuction returns false,
				// then the caller will print the close log.
				// Because of that, if close log was not printed
				// yet, we return this fact to the caller (via
				// close_printed) and already here set it as
				// printed.
				if (!tmp->last_open.close_printed)
					tmp->last_open.close_printed = true;
			} else {
				// set true even though the event may have not
				// been printed - but we want only the
				// "original" fd to have its close event
				close_printed = true;
			}

			// We also remove in advance the fd from last_open.fds
			// list to improve performance a bit (since, again, we
			// assume that the close event on this fd is currently
			// happening).
			tmp->last_open.fds[idx] = -1;
			tmp->last_open.fds_count--;
			pr_debug("removing fd %d, fds_count is %d",
				 fd, tmp->last_open.fds_count);

			fd_found = true;
			break;

		}
		// if the fd was not found on the fds list, that means that it
		// isn't related to last_open file (see comments for
		// struct last_open_t).
		if (!fd_found)
			close_printed = false;
	}

	mutex_unlock(&list_mutex);
	return close_printed;
}


// ########## UTILITIES ##########

static void print_long_string(u64 upid, const char* buffer, unsigned long size,
			      char* prefix, struct task_struct* task)
{
	unsigned idx = 0;
	unsigned remaining = size;
	char* pos = (char*) buffer;
	char* new_line = strnchr(buffer, size, '\n');

	if (size < MAX_STR_LEN_PER_LINE) {
		if (!new_line) {  // fast path
			PRINT_TRACE_TASK(upid, task, "%s|%s", prefix, buffer);
			return;
		} else {
			char* buf_iter, *buf_cpy, *buf_cpy_start;

			buf_cpy = kstrndup(buffer, size, GFP_KERNEL);
			if (!buf_cpy) {
				pr_err("print_long_string: kstrndup failed\n");
				return;
			}
			buf_cpy_start = buf_cpy;
			buf_iter = strsep(&buf_cpy, "\n");
			PRINT_TRACE_TASK(upid, task, "%s|%s", prefix, buf_iter);
			while ((buf_iter = strsep(&buf_cpy, "\n")) != NULL) {
				PRINT_TRACE_TASK(upid, task,
						 "!Cont|%s", buf_iter);
			}
			PRINT_TRACE_TASK(upid, task, "!Cont_end|");
			kfree(buf_cpy_start);
			return;
		}
	}

	if (!new_line) {  // fast path
		while (remaining >= MAX_STR_LEN_PER_LINE) {
			PRINT_TRACE_TASK(upid, task,
					"%s[%d]%."XSTR(MAX_STR_LEN_PER_LINE)"s",
					prefix, idx, pos);
			pos += MAX_STR_LEN_PER_LINE;
			remaining -= MAX_STR_LEN_PER_LINE;
			idx++;
		}

		if (remaining) {
			PRINT_TRACE_TASK(upid, task,
					"%s[%d]%s", prefix, idx, pos);
		}

	} else {
		bool cont = false;
		bool should_print_cont_end = false;
		char* buf_iter, *buf_cpy, *buf_cpy_start;
		buf_cpy = buf_cpy_start = kstrndup(buffer, size, GFP_KERNEL);
		if (!buf_cpy) {
			pr_err("print_long_string: kstrndup failed\n");
			return;
		}

		// go over every (potentially long) line of the string
		while ((buf_iter = strsep(&buf_cpy, "\n")) != NULL) {
			size_t buf_iter_len = strlen(buf_iter);
			pos = buf_iter;
			while (buf_iter_len) {
				if (cont) {
					// this is not the first line of string
					PRINT_TRACE_TASK(upid, task,
							 "!Cont|%."XSTR(MAX_STR_LEN_PER_LINE)"s",
							 pos);

					// don't print "!Cont" for the rest of
					// this line. The only reason a single
					// line would have multiple trace
					// entries, is that the line is longer
					// than MAX_STR_LEN_PER_LINE, and in
					// that case simply print it with
					// regular tag. E.g. for
					// "aaa\nbbbbbb....bbbbb" print:
					// xx[0]aaa
					// !Cont|bbbbb...bbb
					// xx[1]bbbb
					cont = false;
					should_print_cont_end = true;
				} else {
					if (should_print_cont_end) {
						PRINT_TRACE_TASK(upid, task,
								 "!Cont_end|");
						should_print_cont_end = false;
					}
					PRINT_TRACE_TASK(upid, task,
							 "%s[%d]%."XSTR(MAX_STR_LEN_PER_LINE)"s",
							 prefix, idx, pos);
					idx++;
				}
				pos += min_t(size_t,
					     buf_iter_len,
					     MAX_STR_LEN_PER_LINE);
				buf_iter_len -= min_t(size_t,
						      buf_iter_len,
						      MAX_STR_LEN_PER_LINE);
			}
			// set cont to true for the second and later lines in
			// the string
			cont = true;
			if (should_print_cont_end) {
				PRINT_TRACE_TASK(upid, task, "!Cont_end|");
				should_print_cont_end = false;
			}
		}
		kfree(buf_cpy_start);
	}

	PRINT_TRACE_TASK(upid, task, "%s_end|", prefix);
}

// caller is responsible for freeing tmp_buf
static char* get_pathstr_from_path(struct path* p, char** tmp_buf)
{
	int i;
	unsigned long trysize = PATH_MAX;
	char* tmpbuf, *retbuf;

	for (i = 0; i < 5; i++) {
		tmpbuf = kmalloc(trysize, GFP_KERNEL);
		if (!tmpbuf) {
			pr_warn("get_pathstr_from_path: unable to allocate %ld bytes for fname\n",
				trysize);
			return NULL;
		}
		retbuf = d_path(p, tmpbuf, trysize);
		if (IS_ERR(retbuf)) {
			kfree(tmpbuf);
			trysize *= 2;
			continue;
		} else {
			break;
		}
	}

	if (i == 6) {
		pr_warn("get_pathstr_from_path: path too long!?\n");
		kfree(tmpbuf);
		return NULL;
	}

	*tmp_buf = tmpbuf;
	return retbuf;
}


// ########## TRACING FUNCTIONS ##########

static void tracepoint_probe_fork(void* data, struct task_struct* self,
				  struct task_struct *task)
{
	u64 upid = INVALID_UPID;
	u64 pupid = INVALID_UPID;

	if (!task || !self) {
		pr_warn("tracepoint_probe_fork: null task_struct\n");
		return;
	}

	pr_debug("fork pid: %d, ppid: %d\n",(int) task->pid,(int) self->pid);
	upid = maybe_add_pid_to_list(task->pid, self->pid, false);

	if (!should_trace(&pupid))
		return;

	PRINT_TRACE_TASK(pupid, self, "!SchedFork|pid=%lld",
			UNIQUE_PID_FOR_TASK(upid, task));
}

static void tracepoint_probe_exit(void* data, struct task_struct *task)
{
	u64 upid = INVALID_UPID;

	if (!task) {
		pr_warn("tracepoint_probe_exit: null task_struct\n");
		return;
	}

	if (should_trace(&upid)) {
		PRINT_TRACE_TASK(upid, task, "!Exit|");
	}

	pr_debug("exit: %d\n", (int) task->pid);
	remove_pid_from_list(task->pid);
}

static unsigned long print_args_from_buffer(u64 upid, char* buffer, size_t size,
					    unsigned long arg_num,
					    struct task_struct* task)
{
	char* ptr = buffer;
	char* print_ptr, *new_line;
	char* endptr = buffer + size;
	size_t len, tmp_len;
	bool cont;
	bool should_print_cont_end;
	char fmt[64];

	while (ptr < endptr) {
		len = strnlen(ptr, size);
		tmp_len = len;
		print_ptr = ptr;
		cont = false;

		// trace_pipe buffer accepts lines up to 1024 bytes long. Put
		// max MAX_STR_LEN_PER_LINE bytes of argument at a time
		while (tmp_len) {
			size_t part_len = tmp_len > MAX_STR_LEN_PER_LINE ?
						MAX_STR_LEN_PER_LINE : tmp_len;
			new_line = strnchr(print_ptr, part_len, '\n');
			if (new_line) {
				part_len = min_t(size_t,
						 part_len,
						 (size_t)(new_line - print_ptr));
			}

			if (cont) {
				SNPRINTF_TRACE(fmt, 64, "!Cont|%%.%lds",
					       part_len);
				PRINT_TRACE_TASK_RAW(upid, task, fmt, print_ptr);
				should_print_cont_end = true;
			} else {
				if (should_print_cont_end) {
					PRINT_TRACE_TASK(upid, task,
							 "!Cont_end|");
					should_print_cont_end = false;
				}
				SNPRINTF_TRACE(fmt, 64, "!A[%%ld]%%.%lds",
					       part_len);
				PRINT_TRACE_TASK_RAW(upid, task, fmt, arg_num,
						     print_ptr);
			}

			tmp_len -= part_len;
			print_ptr += part_len;
			if (new_line) {
				cont = true;
				// skip '\n'
				if (tmp_len) {
					tmp_len--; // TODO can this ever be 0--?
					print_ptr++;
				} else {
					pr_warn("print_args_from_buffer: attempting tmp_len-- on tmp_len == 0\n");
				}
			} else {
				cont = false;
			}
		}

		ptr += len;

		if (should_print_cont_end) {
			if (*(ptr-1) == '\n') {
				// this is the case when we have '\n' at the end
				// of an argument. Print an empty line here to
				// be consistent with how other strings are
				// printed and to not lose info about that '\n'.
				PRINT_TRACE_TASK(upid, task, "!Cont|");
			}
			PRINT_TRACE_TASK(upid, task, "!Cont_end|");
			should_print_cont_end = false;
		}

		ptr += 1; // skip null-byte
		size -= (len + 1);
		arg_num++;
	}
	if (*(endptr-1)) {
		// the last character is not \0, so we are probably in the
		// middle of an argument splitted between multiple pages
		// (buffers). Don't increment arg_num in that case.
		arg_num--;
	}
	return arg_num;
}

static void print_exec_args(u64 upid, struct task_struct* p,
			    unsigned long arg_start, unsigned long args_size)
{
	char* page;
	unsigned long arg_num, left, pos;
	int got;

	page = (char*)__get_free_page(GFP_KERNEL);
	if (!page) {
		pr_err("print_exec_args: get_free_page() failed\n");
		return;
	}

	left = args_size;
	pos = arg_start;
	arg_num = 0;
	while (left) {
		size_t size = min_t(size_t, PAGE_SIZE, left);
		got = access_process_vm(p, pos, page, size, FOLL_ANON);
		if (got < 0) {
			pr_err("print_exec_args: access_process_vm failed\n");
			free_page((unsigned long) page);
			return;
		}
		if (got == 0)
			break;

		arg_num = print_args_from_buffer(upid, page, size, arg_num, p);

		pos += got;
		left -= got;
	}
	PRINT_TRACE_TASK(upid, p, "!End_of_args|");

	free_page((unsigned long) page);
}

// based on fs/proc/base.c:get_mm_cmdline()
static void tracepoint_probe_exec(void* data, struct task_struct *p,
				  pid_t old_pid, struct linux_binprm *bprm)
{
	u64 upid = INVALID_UPID;

	struct mm_struct *mm;
	unsigned long arg_start, arg_end, args_size, interp_size,
		      filename_size, cwd_size;
	struct path cwd_path, pp_path, pi_path;
	int err;
	char *pp_path_buf = NULL, *pp_tmp_buf = NULL,
	     *pi_path_buf = NULL, *pi_tmp_buf = NULL;
	char *cwd_tmp_buf = NULL, *cwd_path_buf = NULL;

	// TODO: assuming that this tracepoint is always called from the context
	// of newly created process, check it
	if (!should_trace(&upid))
		return;

	pr_debug("tracepoint_probe_exec callback called\n");

	if (!p) {
		pr_err("tracepoint_probe_exec: task_struct is null\n");
		return;
	}

	if (!bprm) {
		pr_err("tracepoint_probe_exec: bprm is null\n");
		return;
	}

	mm = p->mm; // TODO should use get_task_mm()?
	if (!mm) {
		pr_err("tracepoint_probe_exec: mm is null\n");
		return;
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(5,8,0)
	spin_lock(&mm->arg_lock);
#else
	down_read(&mm->mmap_sem);
#endif
	arg_start = mm->arg_start;
	arg_end = mm->arg_end;
#if LINUX_VERSION_CODE > KERNEL_VERSION(5,8,0)
	spin_unlock(&mm->arg_lock);
#else
	up_read(&mm->mmap_sem);
#endif

	if (arg_start >= arg_end)
		return;

	args_size = arg_end - arg_start;
	pr_debug("args start: %lx, end: %lx, len: %ld\n",
		 arg_start, arg_end, args_size);

	task_lock(p);
	if (p->fs) {
		get_fs_pwd(p->fs, &cwd_path);
		cwd_path_buf = get_pathstr_from_path(&cwd_path, &cwd_tmp_buf);
	}
	task_unlock(p);

	if (!cwd_path_buf) {
		pr_warn("tracepoint_probe_exec: failed to get cwd path\n");
		goto clean_cwd_path;
	}
	cwd_size = strlen(cwd_path_buf);


	// get absolute canonicalized progname path

	err = kern_path(bprm->filename, 0, &pp_path);
	if (err) {
		pr_warn("tracepoint_probe_exec: kern_path(bprm->filename) failed\n");
		goto clean_cwd;
	}
	pp_path_buf = get_pathstr_from_path(&pp_path, &pp_tmp_buf);
	if (!pp_path_buf) {
		pr_warn("tracepoint_probe_exec: failed to get PP path\n");
		goto clean_cwd_pp_path;
	}
	filename_size = strlen(pp_path_buf);

	// get absolute canonicalized interp path

	err = kern_path(bprm->interp, 0, &pi_path);
	if (err) {
		pr_warn("tracepoint_probe_exec: kern_path(bprm->interp) failed\n");
		goto clean_cwd_pp;
	}
	pi_path_buf = get_pathstr_from_path(&pi_path, &pi_tmp_buf);
	if (!pi_path_buf) {
		pr_warn("tracepoint_probe_exec: failed to get PI path\n");
		goto clean_cwd_pp_pi_path;
	}
	interp_size = strlen(pi_path_buf);

	PRINT_TRACE_TASK(upid, p,
			 "!New_proc|argsize=%ld,prognameisize=%ld,prognamepsize=%ld,cwdsize=%ld",
			 args_size, interp_size, filename_size, cwd_size);
	print_long_string(upid, pi_path_buf, interp_size, "!PI", p);
	print_long_string(upid, pp_path_buf, filename_size, "!PP", p);
	print_long_string(upid, cwd_path_buf, cwd_size, "!CW", p);
	print_exec_args(upid, p, arg_start, args_size);


	kfree(pi_tmp_buf);
clean_cwd_pp_pi_path:
	path_put(&pi_path);
clean_cwd_pp:
	kfree(pp_tmp_buf);
clean_cwd_pp_path:
	path_put(&pp_path);
clean_cwd:
	kfree(cwd_tmp_buf);
clean_cwd_path:
	path_put(&cwd_path);
}

static void tracepoint_probe_sys_enter(void* data, struct pt_regs *regs,
				       long id)
{
	u64 upid = INVALID_UPID;

	switch(id) {
	case __NR_clone: {
		unsigned long clone_flags;

		if (!should_trace(&upid))
			return;

		// according to man clone(2) (NOTES), clone syscall takes flags
		// in the first parameter under x86-64
		clone_flags = regs->di;
		PRINT_TRACE(upid, "!SysClone|flags=%ld", clone_flags);
		break;
	}
	case __NR_close: {
		bool should_print = true;
		int fd = (int) regs->di;
		if (!should_trace(&upid))
			return;

		if (ignore_repeated_opens) {
			should_print = !update_last_open_fd_on_close(fd);
		}

		if (should_print) {
			PRINT_TRACE(upid, "!Close|fd=%ld", regs->di);
		}
		break;
	}
	case __NR_rename:
	case __NR_renameat:
	case __NR_renameat2: {
		struct path from;
		char __user *user_str;
		char* path_buf, *tmp_buf;
		int err;
		unsigned long len;

		if (!should_trace(&upid))
			return;

		if (id == __NR_rename) {
			user_str = (char __user*) regs->di;
			err = user_path_at(AT_FDCWD, user_str, 0, &from);
		} else {
			int dir_fd = (int) regs->di;
			user_str = (char __user*) regs->si;
			err = user_path_at(dir_fd, user_str, 0, &from);
		}
		if (err) {
			pr_warn("sys_enter_rename: user_path_at failed\n");
			return;
		}

		path_buf = get_pathstr_from_path(&from, &tmp_buf);
		if (!path_buf) {
			return;
		}
		len = strlen(path_buf);

		if (id == __NR_renameat2) {
			int flags = (int) regs->r8;
			PRINT_TRACE(upid, "!Rename2From|fnamesize=%ld,flags=%d",
				    len, flags);
		} else {
			PRINT_TRACE(upid, "!RenameFrom|fnamesize=%ld", len);
		}
		print_long_string(upid, path_buf, len, "!RF", current);
		kfree(tmp_buf);
		break;
	}
	case __NR_link:
	case __NR_linkat: {
		struct path from;
		char __user *user_str;
		char* path_buf, *tmp_buf;
		int err;
		unsigned long len;

		if (!should_trace(&upid))
			return;

		if (id == __NR_link) {
			user_str = (char __user*) regs->di;
			err = user_path_at(AT_FDCWD, user_str, 0, &from);
		} else {
			int dir_fd = (int) regs->di;
			user_str = (char __user*) regs->si;
			err = user_path_at(dir_fd, user_str, 0, &from);
		}
		if (err) {
			pr_warn("sys_enter_link: user_path_at failed\n");
			return;
		}

		path_buf = get_pathstr_from_path(&from, &tmp_buf);
		if (!path_buf) {
			return;
		}
		len = strlen(path_buf);

		if (id == __NR_linkat) {
			int flags = (int) regs->r8;
			PRINT_TRACE(upid, "!LinkatFrom|fnamesize=%ld,flags=%d",
				    len, flags);
		} else {
			PRINT_TRACE(upid, "!LinkFrom|fnamesize=%ld", len);
		}
		print_long_string(upid, path_buf, len, "!LF", current);
		kfree(tmp_buf);
		break;
	}
	} // switch
}

static void tracepoint_probe_sys_exit(void *data, struct pt_regs *regs,
				      long ret)
{
	u64 upid = INVALID_UPID;

	struct files_struct* fs;
	struct file *f;
	char *retbuf, *tmpbuf;
	int flags, mode, i;
	long trysize = PATH_MAX, pathsize;

	long syscall_nr = syscall_get_nr(current, regs);

	// TODO support creat()?
	switch(syscall_nr) {
	case __NR_clone: {
		if (!should_trace(&upid))
			return;

		if (ret < 0) {
			PRINT_TRACE(upid, "!SysCloneFailed|");
			return;
		}
		break;
	}
	case __NR_open:
	case __NR_openat: {
		bool should_print = true;
		char __user *user_orig_path;
		char *dir_path_tmpbuf = NULL, *dir_path_retbuf = NULL,
			 *orig_path = NULL, *final_orig_path = NULL;
		struct path dir_path;
		size_t orig_path_len, dir_path_len, final_orig_path_size;
	   	int dir_fd;

		if (!should_trace(&upid))
			return;

		if (ret < 0)
			return;

		task_lock(current);
		fs = current->files;
		// TODO add synchronization (see fs/proc/fd.c:proc_fd_link())
		task_unlock(current);

		if (!fs) {
			pr_warn("sys_exit_open: failed to get file_struct of task\n");
			// todo synchronization
			return;
		}

		// TODO check fd boundary
		f = fs->fdt->fd[ret];
		if (!f) {
			pr_warn("sys_exit_open: failed to get struct file for desc %ld\n", ret);
			// todo synchronization
			return;
		}

		for (i = 0; i < 5; i++) {
			tmpbuf = kmalloc(trysize, GFP_KERNEL);
			if (!tmpbuf) {
				pr_warn("sys_exit_open: unable to allocate %ld bytes for fname\n", trysize);
				// todo synchronization
				return;
			}
			retbuf = d_path(&f->f_path, tmpbuf, trysize);
			if (IS_ERR(retbuf)) {
				kfree(tmpbuf);
				trysize *= 2;
				continue;
			} else {
				break;
			}
		}

		if (i == 6) {
			pr_warn("sys_exit_open: path too long!?\n");
			kfree(tmpbuf);
			// todo synchronization
			return;
		}

		if (syscall_nr == __NR_open) {
			dir_fd = AT_FDCWD;
			user_orig_path = (char __user *) regs->di;
			flags = regs->si;
			mode = regs->dx;
		} else { // __NR_openat
			dir_fd = (int) regs->di;
			user_orig_path = (char __user *) regs->si;
			flags = regs->dx;
			mode = regs->r10;
		}

		pathsize = strlen(retbuf);

		if (ignore_repeated_opens) {
			if (is_repeating_open(retbuf, pathsize,
					      flags, mode, ret)) {
				should_print = false;
			} else {
				update_last_open(retbuf, pathsize, flags, mode,
						 ret);
			}
		}

		if (should_print) {

			// prepare "original user path". If user supplied an
			// absolute path, print it verbatim. If it was relative,
			// resolve the dir_fd and glue it together with the
			// user-provided part.

			orig_path_len = strnlen_user(user_orig_path, 4096*32);
			if (!orig_path_len) {
				pr_warn("sys_exit_open: strnlen_user failed\n");
				goto open_exit;
			}

			orig_path = kmalloc(orig_path_len, GFP_KERNEL);
			if (!orig_path) {
				pr_warn("sys_exit_open: kalloc for orig_path failed\n");
				goto open_exit;
			}

			if (strncpy_from_user(orig_path,
					      user_orig_path,
					      orig_path_len) < 0) {
				pr_warn("sys_exit_open: strncpy_from_user failed\n");
				goto open_exit2;
			}

			if (orig_path_len)
				// strnlen_user() includes NUL byte
				orig_path_len--;

			if (orig_path[0] == '/') {
				final_orig_path = orig_path;
				final_orig_path_size = orig_path_len;
			} else {
				if (dir_fd == AT_FDCWD) {
					task_lock(current);
					if (current->fs) {
						get_fs_pwd(current->fs, &dir_path);
					} else {
						pr_warn("sys_exit_open: current->fs is null\n");
						task_unlock(current);
						goto open_exit2;
					}
					task_unlock(current);
				} else {
					f = fs->fdt->fd[dir_fd];
					if (!f) {
						pr_warn("sys_exit_open: fd[at_dir_fd] is null\n");
						goto open_exit2;
					}
					dir_path = f->f_path;
				}
				dir_path_retbuf = get_pathstr_from_path(&dir_path, &dir_path_tmpbuf);
				if (!dir_path_retbuf) {
					pr_warn("sys_exit_open: failed to resolve at_dir path\n");
					goto open_exit2;
				}

				// join dir path and user string together

				dir_path_len = strlen(dir_path_retbuf);
				// at_dir path + "/" + user-provided path (without NUL)
				final_orig_path_size = dir_path_len + 1 + orig_path_len;
				// alloc one more byte for NUL terminator
				final_orig_path = kmalloc(final_orig_path_size + 1,
							  GFP_KERNEL);
				if (!final_orig_path) {
					pr_warn("sys_exit_open: kalloc for final_orig_path failed\n");
					goto open_exit3;
				}

				strncpy(final_orig_path, dir_path_retbuf,
					dir_path_len);
				final_orig_path[dir_path_len] = '/';
				strncpy(final_orig_path + dir_path_len + 1,
					orig_path, orig_path_len);
				final_orig_path[final_orig_path_size] = 0x0;
			}

			PRINT_TRACE(upid,
				    "!Open|fnamesize=%ld,forigsize=%ld,flags=%d,mode=%d,fd=%ld",
				    pathsize, final_orig_path_size, flags,
				    mode, ret);
			print_long_string(upid, retbuf, pathsize, "!FN",
					  current);
			print_long_string(upid, final_orig_path,
					  final_orig_path_size, "!FO", current);
		}

		if (final_orig_path && final_orig_path != orig_path)
			kfree(final_orig_path);
open_exit3:
		if (dir_path_tmpbuf)
			kfree(dir_path_tmpbuf);
open_exit2:
		if (orig_path)
			kfree(orig_path);
open_exit:
		// todo synchronization
		kfree(tmpbuf);
		break;
	}
	case __NR_pipe:
	case __NR_pipe2: {
		int __user *fd_arr;
		int fd1, fd2, err, flags;

		if (!should_trace(&upid))
			return;

		if (ret < 0) {
			return;
		}
		fd_arr = (int __user*) regs->di;
		err = get_user(fd1, fd_arr);
		if (err) {
			pr_warn("sys_exit_pipe: couldn't read fd1\n");
			return;
		}
		err = get_user(fd2, fd_arr + 1);
		if (err) {
			pr_warn("sys_exit_pipe: couldn't read fd2\n");
			return;
		}

		if (syscall_nr == __NR_pipe) {
			// from man pipe(2): If flags is 0, then pipe2() is the
			// same as pipe().
			flags = 0;
		} else {
			flags = (int) regs->si;
		}

		PRINT_TRACE(upid, "!Pipe|fd1=%d,fd2=%d,flags=%d",
			    fd1, fd2, flags);
		break;
	}
	case __NR_rename:
	case __NR_renameat:
	case __NR_renameat2: {
		struct path to;
		char __user *user_str;
		char* path_buf, *tmp_buf;
		int err;
		unsigned long len;

		if (!should_trace(&upid))
			return;

		if (ret < 0) {
			PRINT_TRACE(upid, "!RenameFailed|");
			return;
		}

		if (syscall_nr == __NR_rename) {
			user_str = (char __user*) regs->si;
			err = user_path_at(AT_FDCWD, user_str, 0, &to);
		} else {
			int dir_fd = (int) regs->dx;
			user_str = (char __user*) regs->r10;
			err = user_path_at(dir_fd, user_str, 0, &to);
		}

		if (err) {
			pr_warn("sys_exit_rename: user_path_at failed\n");
			return;
		}

		path_buf = get_pathstr_from_path(&to, &tmp_buf);
		if (!path_buf) {
			return;
		}
		len = strlen(path_buf);

		PRINT_TRACE(upid, "!RenameTo|fnamesize=%ld", len);
		print_long_string(upid, path_buf, len, "!RT", current);
		kfree(tmp_buf);

		break;
	}
	case __NR_link:
	case __NR_linkat: {
		struct path to;
		char __user *user_str;
		char* path_buf, *tmp_buf;
		int err;
		unsigned long len;

		if (!should_trace(&upid))
			return;

		if (ret < 0) {
			PRINT_TRACE(upid, "!LinkFailed|");
			return;
		}

		if (syscall_nr == __NR_link) {
			user_str = (char __user*) regs->si;
			err = user_path_at(AT_FDCWD, user_str, 0, &to);
		} else {
			int dir_fd = (int) regs->dx;
			user_str = (char __user*) regs->r10;
			err = user_path_at(dir_fd, user_str, 0, &to);
		}

		if (err) {
			pr_warn("sys_exit_link: user_path_at failed\n");
			return;
		}

		path_buf = get_pathstr_from_path(&to, &tmp_buf);
		if (!path_buf) {
			return;
		}
		len = strlen(path_buf);

		PRINT_TRACE(upid, "!LinkTo|fnamesize=%ld", len);
		print_long_string(upid, path_buf, len, "!LT", current);
		kfree(tmp_buf);

		break;
	}
	case __NR_symlink:
	case __NR_symlinkat: {
		struct path target, to;
		char __user *user_str, __user *user_target_str;
		char *path_buf, *tmp_buf, *resolved_target_tmp_buf,
		     *resolved_target_str, *target_str;
		int err, target_resolve_err, dir_fd;
		unsigned long len, target_len, resolved_target_len;
		bool target_resolved = false;

		if (!should_trace(&upid))
			return;

		if (ret < 0) {
			return;
		}

		if (syscall_nr == __NR_symlink) {
			user_str = (char __user*) regs->si;
			dir_fd = AT_FDCWD;
		} else {
			user_str = (char __user*) regs->dx;
			dir_fd = (int) regs->si;
		}
		err = user_path_at(dir_fd, user_str, 0, &to);

		if (err) {
			pr_warn("sys_exit_symlink: user_path_at(to) failed\n");
			return;
		}

		user_target_str = (char __user *) regs->di;

		target_len = strnlen_user(user_target_str, 4096*32);
		if (!target_len) {
			pr_warn("sys_exit_symlink: strnlen_user failed\n");
			return;
		}

		target_str = kmalloc(target_len, GFP_KERNEL);
		if (!target_str) {
			pr_warn("sys_exit_symlink: kalloc for target string failed\n");
			return;
		}

		if (strncpy_from_user(target_str,
				      user_target_str, target_len) < 0) {
			pr_warn("sys_exit_symlink: strncpy_from_user failed\n");
			kfree(target_str);
			return;
		}

		if (target_len)
			target_len--; // strnlen_user() includes NUL byte


		// try to resolve target by reading the newly created link
		target_resolve_err = user_path_at(dir_fd, user_str,
						  LOOKUP_FOLLOW | LOOKUP_CREATE,
						  &target);
		if (target_resolve_err) {
			pr_warn("sys_exit_symlink: user_path_at(target) failed\n");
		} else {
			resolved_target_str = get_pathstr_from_path(&target, &resolved_target_tmp_buf);
			if (resolved_target_str) {
				resolved_target_len = strlen(resolved_target_str);
				target_resolved = true;
			}
		}

		path_buf = get_pathstr_from_path(&to, &tmp_buf);
		if (!path_buf) {
			if (target_resolved)
				kfree(resolved_target_tmp_buf);
			return;
		}
		len = strlen(path_buf);

		if (target_resolved) {
			PRINT_TRACE(upid,
				    "!Symlink|targetnamesize=%ld,resolvednamesize=%ld,linknamesize=%ld",
				    target_len, resolved_target_len, len);
			print_long_string(upid, target_str, target_len, "!ST",
					  current);
			print_long_string(upid, resolved_target_str,
					  resolved_target_len, "!SR", current);
			print_long_string(upid, path_buf, len, "!SL", current);

			kfree(resolved_target_tmp_buf);
		} else {
			PRINT_TRACE(upid,
				    "!Symlink|targetnamesize=%ld,linknamesize=%ld",
				    target_len, len);
			print_long_string(upid, target_str, target_len, "!ST",
					  current);
			print_long_string(upid, path_buf, len, "!SL", current);
		}

		kfree(tmp_buf);
		break;
	}
	case __NR_dup:
	case __NR_dup2:
	case __NR_dup3: {
		int old_fd;
		int flags = 0;

		if (!should_trace(&upid))
			return;

		if (ret < 0) {
			return;
		}

		if (syscall_nr == __NR_dup3) {
			flags = (int) regs->dx;
		}

		old_fd = (int) regs->di;
		PRINT_TRACE(upid, "!Dup|oldfd=%d,newfd=%d,flags=%d",
			    old_fd, (int) ret, flags);
		break;
	}
	case __NR_fcntl: {
		int cmd = (int) regs->si;
		int flags = 0;
		int old_fd;

		// fast path
		if (cmd != F_DUPFD && cmd != F_DUPFD_CLOEXEC)
			return;

		if (!should_trace(&upid))
			return;

		if (ret < 0) {
			return;
		}

		if (cmd == F_DUPFD_CLOEXEC) {
			flags = O_CLOEXEC;
		}

		old_fd = (int) regs->di;
		PRINT_TRACE(upid, "!Dup|oldfd=%d,newfd=%d,flags=%d",
			    old_fd, (int) ret, flags);
		break;
	}
	} // switch
}


// ########## SETUP AND CLEANUP ##########

static void register_tracepoints(struct tracepoint *tp, void *ignore)
{
	if (!strncmp(tp->name,
		     "sched_process_fork",
		     strlen("sched_process_fork"))) {
		pr_info("Attaching to tracepoint: %s\n", tp->name);
		tracepoint_sched_process_fork = tp;
		tracepoint_probe_register(tp, tracepoint_probe_fork, NULL);
	} else if (!strncmp(tp->name,
			    "sched_process_exit",
			    strlen("sched_process_exit"))) {
		pr_info("Attaching to tracepoint: %s\n", tp->name);
		tracepoint_sched_process_exit = tp;
		tracepoint_probe_register(tp, tracepoint_probe_exit, NULL);
	} else if (!strncmp(tp->name,
			    "sched_process_exec",
			    strlen("sched_process_exec"))) {
		pr_info("Attaching to tracepoint: %s\n", tp->name);
		tracepoint_sched_process_exec = tp;
		tracepoint_probe_register(tp, tracepoint_probe_exec, NULL);
	} else if (!strncmp(tp->name,
			    "sys_enter",
			    strlen("sys_enter"))) {
		pr_info("Attaching to tracepoint: %s\n", tp->name);
		tracepoint_sys_enter = tp;
		tracepoint_probe_register(tp, tracepoint_probe_sys_enter, NULL);
	} else if (!strncmp(tp->name,
			    "sys_exit",
			    strlen("sys_exit"))) {
		pr_info("Attaching to tracepoint: %s\n", tp->name);
		tracepoint_sys_exit = tp;
		tracepoint_probe_register(tp, tracepoint_probe_sys_exit, NULL);
	}
}


static int et_init(void)
{
	pr_info("et_init called\n");

	if (ignore_repeated_opens) {
		pr_info("Repeated open() calls will be ignored");
	}

	// check if bits_of_real_pid is sane

	if (bits_of_real_pid < 0 || bits_of_real_pid > 32) {
		pr_err("Invalid bits_of_real_pid value. Acceptable values are [0, 32].");
		return -EINVAL;
	}

	// read root pid from parameter

	if (!root_pid) {
		pr_err("Invalid root_pid value (pass root_pid=<pid> when insmod'ing)\n");
		return -EINVAL;
	}
	maybe_add_pid_to_list((pid_t) root_pid, (pid_t) 0/* don't care */, true);

	// set up tracepoints for tracing forks/exits

	for_each_kernel_tracepoint(register_tracepoints, NULL);

	if (!tracepoint_sched_process_fork)
		pr_warn("Not attached to sched_process_fork tracepoint");

	if (!tracepoint_sched_process_exit)
		pr_warn("Not attached to sched_process_exit tracepoint");

	if (!tracepoint_sched_process_exec)
		pr_warn("Not attached to sched_process_exec tracepoint");

	if (!tracepoint_sys_enter)
		pr_warn("Not attached to sys_enter tracepoint");

	if (!tracepoint_sys_exit)
		pr_warn("Not attached to sys_exit tracepoint");

	pr_info("Module loaded\n");

	return 0;
}
module_init(et_init);

static void et_exit(void)
{
	pr_info("et_exit called\n");

	if (tracepoint_sched_process_fork)
		tracepoint_probe_unregister(tracepoint_sched_process_fork,
					    tracepoint_probe_fork, NULL);

	if (tracepoint_sched_process_exit)
		tracepoint_probe_unregister(tracepoint_sched_process_exit,
					    tracepoint_probe_exit, NULL);

	if (tracepoint_sched_process_exec)
		tracepoint_probe_unregister(tracepoint_sched_process_exec,
					    tracepoint_probe_exec, NULL);

	if (tracepoint_sys_enter)
		tracepoint_probe_unregister(tracepoint_sys_enter,
					    tracepoint_probe_sys_enter, NULL);

	if (tracepoint_sys_exit)
		tracepoint_probe_unregister(tracepoint_sys_exit,
					    tracepoint_probe_sys_exit, NULL);

	clear_pids_list();
	pr_info("Module unloaded\n");
}
module_exit(et_exit);

