#define pr_fmt(fmt) "bas_tracer: " fmt

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
#include <linux/radix-tree.h>
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

#if defined(GIT_COMMIT) && defined(COMPILATION_TIME)
#define TRACER_VERSION GIT_COMMIT ", compiled " COMPILATION_TIME
#else
#define TRACER_VERSION "unknown"
#warning "Tracer version info unavailable"
#endif

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
		preempt_disable_notrace();  \
		my_trace_printk(fmt,  \
			UNIQUE_PID_FOR_TASK(upid, tsk), smp_processor_id(), \
			ts.tv_sec, ts.tv_nsec, ##__VA_ARGS__);  \
		preempt_enable_notrace();  \
	} while (0)
#define PRINT_TRACE_TASK(upid, tsk, fmt, ...) \
	PRINT_TRACE_TASK_RAW(upid, tsk, "%lld,%d,%lld,%ld" fmt "\n", ##__VA_ARGS__)
#else
// if called directly, fmt should always come from SNPRINTF_TRACE
#define PRINT_TRACE_TASK_RAW(upid, tsk, fmt, ...) \
	do {  \
		struct timespec ts;  \
		ktime_get_ts(&ts);  \
		preempt_disable_notrace();  \
		my_trace_printk(fmt,  \
			UNIQUE_PID_FOR_TASK(upid, tsk), smp_processor_id(), \
			ts.tv_sec, ts.tv_nsec, ##__VA_ARGS__);  \
		preempt_enable_notrace();  \
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
static int support_ns_pid = 0;
module_param(support_ns_pid, int, 0);

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
	// mutex that guards access to non-const fields in this struct, except
	// list_head. It is locked when this object is returned from the list
	// via should_trace() and must be released no later than when returning
	// from trace functions. While normally there should be only one user of
	// any given object of this type (the thread with given pid), there is
	// a need for synchronization when someone unloads the module while
	// there are still traced tasks.
	struct mutex obj_mutex;
	struct last_open_t last_open;  // protected by obj_mutex
};

static RADIX_TREE(traced_pids, GFP_KERNEL);

static struct tracepoint *tracepoint_sched_process_fork = NULL;
static struct tracepoint *tracepoint_sched_process_exit = NULL;
static struct tracepoint *tracepoint_sched_process_exec = NULL;
static struct tracepoint *tracepoint_sys_enter = NULL;
static struct tracepoint *tracepoint_sys_exit = NULL;


// ########## PIDS LIST MANAGEMENT ##########

static bool is_pid_on_list_locked(pid_t pid, struct traced_pid **tpid)
{
	struct traced_pid *tmp = NULL;
	tmp = radix_tree_lookup(&traced_pids, pid);
	if (tmp) {
		if (tpid) {
			BUG_ON(mutex_is_locked(&tmp->obj_mutex));
			mutex_lock(&tmp->obj_mutex);
			*tpid = tmp;
		}
		return true;
	}
	return false;
}

// If true is returned, tpid will be set (if it is not null) to
// struct traced_pid of current process and its obj_lock will be acquired.
// If false is returned, tpid is untouched.
static bool should_trace(struct traced_pid **tpid)
{
	bool retval = false;
	struct traced_pid *ret_traced_pid = NULL;

	mutex_lock(&list_mutex);
	retval = is_pid_on_list_locked(current->pid, &ret_traced_pid);
	mutex_unlock(&list_mutex);

	if (retval && tpid)
		*tpid = ret_traced_pid;

	return retval;
}

static u64 maybe_add_pid_to_list_locked(pid_t pid, pid_t ppid, pid_t nspid, bool force)
{
	struct traced_pid *new_pid;
	bool should_add = false;
	int retval;

	// force == true should only be used to add the root pid
	if (force)
		should_add = true;
	else
		// check if parent pid is tracked
		should_add = is_pid_on_list_locked(ppid, NULL) || (nspid == (pid_t) root_pid);

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
	mutex_init(&new_pid->obj_mutex);
	retval = radix_tree_insert(&traced_pids, pid, new_pid);
	if (retval == -EEXIST) {
		struct traced_pid *existing;
		existing = radix_tree_lookup(&traced_pids, pid);
		if (!existing) {
			pr_warn("Failed adding pid %d due to EEXIST, but no entry in tree\n",
				pid);
		} else {
			pr_warn("Failed adding pid %d - EEXIST. Upid of duplicate: %lld\n",
				pid, existing->upid);
		}
	}
	if (retval) {
		pr_err("Failed to insert traced_pid into the tree (%d)\n",
		       retval);
		return INVALID_UPID;
	}
	return new_pid->upid;
}

static void remove_pid_from_list_locked(pid_t pid)
{
	struct traced_pid *el;
	el = radix_tree_delete(&traced_pids, pid);
	if (el) {
		pr_debug("Removing pid %d from list\n", (int) pid);
		// we assume that the process (task) cannot be in kernel mode
		// simultaneously on different CPUs / from different contexts,
		// so nothing should be holding mutex_last_open and modify
		// last_open struct
		BUG_ON(mutex_is_locked(&el->obj_mutex));
		mutex_destroy(&el->obj_mutex);
		kfree(el);
	}
}

static void clear_pids_list_locked(void)
{
	struct traced_pid *el;
	struct radix_tree_iter iter;
	void **slot;
	pr_info("Clearing pids list\n");
	radix_tree_for_each_slot(slot, &traced_pids, &iter, 0) {
		// this method is called asynchronously on module's unload, so
		// before removing objects from the list and destroying them,
		// wait for their threads to release them
		el = *slot;
		mutex_lock(&el->obj_mutex);
		WARN_ON(!radix_tree_delete(&traced_pids, el->pid));
		mutex_unlock(&el->obj_mutex);
		mutex_destroy(&el->obj_mutex);
		kfree(el);
	}
}

static u64 maybe_add_pid_to_list(pid_t pid, pid_t ppid, pid_t nspid, bool force)
{
	u64 upid = INVALID_UPID;
	mutex_lock(&list_mutex);
	upid = maybe_add_pid_to_list_locked(pid, ppid, nspid, force);
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

static void release_traced_pid(struct traced_pid *tp)
{
	if (tp)
		mutex_unlock(&tp->obj_mutex);
}

// ########## IGNORING REPEATING OPENS ##########
// caller must hold tpid->obj_mutex to call any function in this section

static bool is_repeating_open(struct traced_pid *tpid, char* path,
			      unsigned long path_size, int flags,
			      int mode, int fd)
{
	bool is_repeating = false;

	if (tpid->last_open.mode == mode
	    && tpid->last_open.flags == flags) {
		unsigned long str_offset = 0;

		if (path_size >= MAX_REP_OPEN_PATH_LEN) {
			str_offset = path_size - (MAX_REP_OPEN_PATH_LEN - 1);
		}

		if (!strncmp(tpid->last_open.fname,
				path + str_offset,
				MAX_REP_OPEN_PATH_LEN - 1)) {
			int iter, old_fds_count;
			is_repeating = true;

			// add fd to the list of fds for this open.
			// This fd shouldn't be already on the list,
			// because !Close event should have removed it
			// if it was used before

			if (tpid->last_open.fds_count >= MAX_REP_OPEN_FDS) {
				pr_warn("Exceeded number of stored fds in last_open_t!");
				return is_repeating;
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
			if (tpid->last_open.fds_count == MAX_REP_OPEN_FDS - 1
			    && tpid->last_open.fds[0] == -1) {
				pr_debug("Hit [open()*MAX -> close(fd[0]) -> open()] edge case");
				return is_repeating;
			}

			old_fds_count = tpid->last_open.fds_count;
			// put the fd in the first empty slot, starting from
			// second array entry (first entry is special, see
			// struct last_open_t docs)
			for (iter = 1; iter < MAX_REP_OPEN_FDS; iter++) {
				if (tpid->last_open.fds[iter] == -1) {
					tpid->last_open.fds[iter] = fd;
					tpid->last_open.fds_count++;
					pr_debug("adding fd %d, fds_count = %d",
						 fd,
						 tpid->last_open.fds_count);
					break;
				}
			}
			if (old_fds_count == tpid->last_open.fds_count) {
				pr_err("Bug!: didn't insert fd on list with fds_count < MAX");
			}
		}
	}
	return is_repeating;
}

static void update_last_open(struct traced_pid *tpid, char* path,
			     unsigned long path_size, int flags,
			     int mode, int fd)
{
	unsigned long str_offset = 0;
	int iter;

	// only copy the last MAX_REP_OPEN_PATH_LEN-1 chars
	if (path_size >= MAX_REP_OPEN_PATH_LEN) {
		str_offset = path_size - (MAX_REP_OPEN_PATH_LEN - 1);
	}

	strscpy(tpid->last_open.fname, path + str_offset,
		MAX_REP_OPEN_PATH_LEN);
	tpid->last_open.flags = flags;
	tpid->last_open.mode = mode;
	tpid->last_open.fds[0] = fd;
	for (iter = 1; iter < MAX_REP_OPEN_FDS; iter++)
		tpid->last_open.fds[iter] = -1;
	tpid->last_open.fds_count = 1;
	tpid->last_open.close_printed = false;
}

/*
 * Updates the last_open cache with regards to !Close event on given fd.
 * Returns true if !Close event for this file was already printed, false otherwise
 */
static bool update_last_open_fd_on_close(struct traced_pid *tpid, int fd)
{
	bool close_printed = false;
	int idx = 0;
	bool fd_found = false;

	if (!tpid->last_open.fds_count) {
		return close_printed; // close_printed = false
	}

	for (; idx < MAX_REP_OPEN_FDS; idx++) {
		if (tpid->last_open.fds[idx] != fd)
			continue;

		// tpid->last_open.fds[idx] == fd

		// Allow potentially printing close event only for the
		// first entry in array (i.e. only the "original" fd
		// from the first open() call may have corresponding
		// close event). This is to maintain consistence between
		// fds printed in open and close events.
		if (!idx) {
			close_printed = tpid->last_open.close_printed;

			// We assume that if this fuction returns false,
			// then the caller will print the close log.
			// Because of that, if close log was not printed
			// yet, we return this fact to the caller (via
			// close_printed) and already here set it as
			// printed.
			if (!tpid->last_open.close_printed)
				tpid->last_open.close_printed = true;
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
		tpid->last_open.fds[idx] = -1;
		tpid->last_open.fds_count--;
		pr_debug("removing fd %d, fds_count is %d",
			 fd, tpid->last_open.fds_count);

		fd_found = true;
		break;

	}
	// if the fd was not found on the fds list, that means that it
	// isn't related to last_open file (see comments for
	// struct last_open_t).
	if (!fd_found)
		close_printed = false;
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

static pid_t get_ns_parrent_pid(struct task_struct *self)
{
	return support_ns_pid ? task_pid_vnr(self) : (pid_t) 0; // root_pid != 0, so never equal
}

// ########## TRACING FUNCTIONS ##########

static void __tracepoint_probe_fork(void* data, struct task_struct* self,
				  struct task_struct *task)
{
	u64 upid = INVALID_UPID;
	struct traced_pid *tppid = NULL;

	if (!task || !self) {
		pr_warn("tracepoint_probe_fork: null task_struct\n");
		return;
	}

	pr_debug("fork pid: %d, ppid: %d\n",(int) task->pid,(int) self->pid);
	upid = maybe_add_pid_to_list(task->pid, self->pid, get_ns_parrent_pid(self), false);

	if (!should_trace(&tppid))
		return;

	PRINT_TRACE_TASK(tppid->upid, self, "!SchedFork|pid=%lld",
			UNIQUE_PID_FOR_TASK(upid, task));

	release_traced_pid(tppid);
}

static void __tracepoint_probe_exit(void* data, struct task_struct *task)
{
	struct traced_pid *tpid = NULL;

	if (!task) {
		pr_warn("tracepoint_probe_exit: null task_struct\n");
		return;
	}

	if (should_trace(&tpid)) {
		PRINT_TRACE_TASK(tpid->upid, task, "!Exit|status=%d",
				 (task->exit_code >> 8) & 0xff);
	}

	pr_debug("exit: %d\n", (int) task->pid);
	release_traced_pid(tpid);
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
static void __tracepoint_probe_exec(void* data, struct task_struct *p,
				  pid_t old_pid, struct linux_binprm *bprm)
{
	struct traced_pid *tpid = NULL;

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
	if (!should_trace(&tpid))
		return;

	pr_debug("tracepoint_probe_exec callback called\n");

	if (!p) {
		pr_err("tracepoint_probe_exec: task_struct is null\n");
		goto out;
	}

	if (!bprm) {
		pr_err("tracepoint_probe_exec: bprm is null\n");
		goto out;
	}

	mm = p->mm; // TODO should use get_task_mm()?
	if (!mm) {
		pr_err("tracepoint_probe_exec: mm is null\n");
		goto out;
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
		goto out;

	args_size = arg_end - arg_start;
	pr_debug("args start: %lx, end: %lx, len: %ld\n",
		 arg_start, arg_end, args_size);

	task_lock(p);
	if (p->fs) {
		get_fs_pwd(p->fs, &cwd_path);
		cwd_path_buf = get_pathstr_from_path(&cwd_path, &cwd_tmp_buf);
		path_put(&cwd_path);
	}
	task_unlock(p);

	if (!cwd_path_buf) {
		pr_warn("tracepoint_probe_exec: failed to get cwd path\n");
		goto out;
	}
	cwd_size = strlen(cwd_path_buf);


	// get absolute canonicalized progname path

	err = kern_path(bprm->filename, 0, &pp_path);
	if (err) {
		pr_warn("tracepoint_probe_exec: kern_path(bprm->filename) failed\n");
		goto out;
	}
	pp_path_buf = get_pathstr_from_path(&pp_path, &pp_tmp_buf);
	path_put(&pp_path);
	if (!pp_path_buf) {
		pr_warn("tracepoint_probe_exec: failed to get PP path\n");
		goto out;
	}
	filename_size = strlen(pp_path_buf);

	// get absolute canonicalized interp path

	err = kern_path(bprm->interp, 0, &pi_path);
	if (err) {
		pr_warn("tracepoint_probe_exec: kern_path(bprm->interp) failed\n");
		goto out;
	}
	pi_path_buf = get_pathstr_from_path(&pi_path, &pi_tmp_buf);
	path_put(&pi_path);
	if (!pi_path_buf) {
		pr_warn("tracepoint_probe_exec: failed to get PI path\n");
		goto out;
	}
	interp_size = strlen(pi_path_buf);

	PRINT_TRACE_TASK(tpid->upid, p,
			 "!New_proc|argsize=%ld,prognameisize=%ld,prognamepsize=%ld,cwdsize=%ld",
			 args_size, interp_size, filename_size, cwd_size);
	print_long_string(tpid->upid, pi_path_buf, interp_size, "!PI", p);
	print_long_string(tpid->upid, pp_path_buf, filename_size, "!PP", p);
	print_long_string(tpid->upid, cwd_path_buf, cwd_size, "!CW", p);
	print_exec_args(tpid->upid, p, arg_start, args_size);


out:
	kfree(pi_tmp_buf);
	kfree(pp_tmp_buf);
	kfree(cwd_tmp_buf);
	release_traced_pid(tpid);
}

static void __tracepoint_probe_sys_enter(void* data, struct pt_regs *regs,
				       long id)
{
	struct traced_pid *tpid = NULL;

	switch(id) {
	case __NR_mount: {
		int err;
		unsigned long flags = regs->r10;
		struct path from, to;
		char __user *source = (char __user *) regs->di;
		char __user *target = (char __user *) regs->si;
		char fstype[256] = {0}; /* fstype is unbound in kernel w*/
		char *tmp = NULL, *tmp2 = NULL;
		int arbitrary_source = 0;

		if (!should_trace(&tpid))
			return;

		err = user_path_at(AT_FDCWD, source, 0, &from);
		if (err) {
			source = kzalloc(PATH_MAX, GFP_KERNEL);
			err = copy_from_user(source, (char __user *) regs->di,
					PATH_MAX - 1);
			arbitrary_source = !arbitrary_source;
		} else {
			source = get_pathstr_from_path(&from, &tmp);
			path_put(&from);
			if (!source) {
				pr_warn("sys_enter_mount: could not get path from"
						" struct path\n");
				break;
			}
		}

		err = user_path_at(AT_FDCWD, target, 0, &to);
		if (err) {
			pr_warn("sys_enter_mount: user_path_at failed\n");
			kfree(tmp);
			kfree(tmp2);
			break;
		}

		target = get_pathstr_from_path(&to, &tmp2);
		path_put(&to);
		if (!target) {
			pr_warn("sys_enter_mount: could not get path from struct"
					" path\n");
			kfree(tmp);
			kfree(tmp2);
			if (arbitrary_source)
				kfree(source);
			break;
		}

		err = strncpy_from_user(fstype, (char __user *) regs->dx,
				sizeof(fstype) - 1);

		if (!err && regs->dx) {
			PRINT_TRACE(tpid->upid, "!Mount|targetnamesize=%ld"
				    ",sourcenamesize=%ld,typenamesize=%ld,"
				    "flags=%ld",
				    strlen(source), strlen(target),
				    strlen(fstype), flags);
		} else {
			PRINT_TRACE(tpid->upid, "!Mount|targetnamesize=%ld"
				    ",sourcenamesize=%ld,flags=%ld",
				    strlen(source), strlen(target), flags);
		}

		print_long_string(tpid->upid, source, strlen(source), "!MS",
				current);
		print_long_string(tpid->upid, target, strlen(target), "!MT",
				current);
		if (!err && regs->dx)
			print_long_string(tpid->upid, fstype, strlen(fstype),
					"!MX", current);
		kfree(tmp);
		kfree(tmp2);
		if (arbitrary_source)
			kfree(source);
		break;
		}
	case __NR_umount2: {
		int flags = regs->si;
		char __user *target = (char __user *) regs->di;
		char *tmp = NULL;
		struct path from;
		int err;

		if (!should_trace(&tpid))
			return;

		err = user_path_at(AT_FDCWD, target, 0, &from);
		if (err) {
			pr_warn("sys_enter_umount: user_path_at failed\n");
			break;
		}

		target = get_pathstr_from_path(&from, &tmp);
		path_put(&from);
		if (!target) {
			pr_warn("sys_enter_umount: could not get path from struct"
				" path\n");
			kfree(tmp);
			break;
		}

		PRINT_TRACE(tpid->upid, "!Umount|targetnamesize=%ld,flags=%d",
			    strlen(target), flags);
		print_long_string(tpid->upid, target, strlen(target), "!MT",
				  current);
		kfree(tmp);
		break;
	}
	case __NR_clone: {
		unsigned long clone_flags;

		if (!should_trace(&tpid))
			return;

		// according to man clone(2) (NOTES), clone syscall takes flags
		// in the first parameter under x86-64
		clone_flags = regs->di;
		PRINT_TRACE(tpid->upid, "!SysClone|flags=%ld", clone_flags);
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

		if (!should_trace(&tpid))
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
			break;
		}

		path_buf = get_pathstr_from_path(&from, &tmp_buf);
		path_put(&from);
		if (!path_buf) {
			break;
		}
		len = strlen(path_buf);

		if (id == __NR_renameat2) {
			int flags = (int) regs->r8;
			PRINT_TRACE(tpid->upid,
				    "!Rename2From|fnamesize=%ld,flags=%d",
				    len, flags);
		} else {
			PRINT_TRACE(tpid->upid, "!RenameFrom|fnamesize=%ld",
				    len);
		}
		print_long_string(tpid->upid, path_buf, len, "!RF", current);
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

		if (!should_trace(&tpid))
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
			break;
		}

		path_buf = get_pathstr_from_path(&from, &tmp_buf);
		path_put(&from);
		if (!path_buf) {
			break;
		}
		len = strlen(path_buf);

		if (id == __NR_linkat) {
			int flags = (int) regs->r8;
			PRINT_TRACE(tpid->upid,
				    "!LinkatFrom|fnamesize=%ld,flags=%d",
				    len, flags);
		} else {
			PRINT_TRACE(tpid->upid, "!LinkFrom|fnamesize=%ld", len);
		}
		print_long_string(tpid->upid, path_buf, len, "!LF", current);
		kfree(tmp_buf);
		break;
	}
	} // switch
	release_traced_pid(tpid);
}

static void __tracepoint_probe_sys_exit(void *data, struct pt_regs *regs,
				      long ret)
{
	struct traced_pid *tpid = NULL;

	struct files_struct* fs;
	struct file *f;
	char *retbuf, *tmpbuf;
	int flags, mode, i;
	long trysize = PATH_MAX, pathsize;

	long syscall_nr = syscall_get_nr(current, regs);

	// TODO support creat()?
	switch(syscall_nr) {
	case __NR_mount: {
		if (!should_trace(&tpid))
			return;

		if (ret < 0) {
			PRINT_TRACE(tpid->upid, "!MountFailed|");
		}

		break;
	}
	case __NR_umount2: {
		if (!should_trace(&tpid))
			return;

		if (ret < 0) {
			PRINT_TRACE(tpid->upid, "!UmountFailed|");
		}

		break;
	}
	case __NR_clone: {
		if (!should_trace(&tpid))
			return;

		if (ret < 0) {
			PRINT_TRACE(tpid->upid, "!SysCloneFailed|");
		}
		break;
	}
	case __NR_close: {
		bool should_print = true;
		int fd = (int) regs->di;
		if (!should_trace(&tpid))
			return;

		if (ret < 0)
			break;

		if (ignore_repeated_opens) {
			should_print = !update_last_open_fd_on_close(tpid, fd);
		}

		if (should_print) {
			PRINT_TRACE(tpid->upid, "!Close|fd=%ld", regs->di);
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

		if (!should_trace(&tpid))
			return;

		if (ret < 0)
			break;

		task_lock(current);
		fs = current->files;
		// TODO add synchronization (see fs/proc/fd.c:proc_fd_link())
		task_unlock(current);

		if (!fs) {
			pr_warn("sys_exit_open: failed to get file_struct of task\n");
			// todo synchronization
			break;
		}

		// TODO check fd boundary
		f = fs->fdt->fd[ret];
		if (!f) {
			pr_warn("sys_exit_open: failed to get struct file for desc %ld\n", ret);
			// todo synchronization
			break;
		}

		for (i = 0; i < 5; i++) {
			tmpbuf = kmalloc(trysize, GFP_KERNEL);
			if (!tmpbuf) {
				pr_warn("sys_exit_open: unable to allocate %ld bytes for fname\n", trysize);
				// todo synchronization
				goto out;
			}

			path_get(&f->f_path);
			retbuf = d_path(&f->f_path, tmpbuf, trysize);
			path_put(&f->f_path);
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
			break;
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
			if (is_repeating_open(tpid, retbuf, pathsize,
					      flags, mode, ret)) {
				should_print = false;
			} else {
				update_last_open(tpid, retbuf, pathsize, flags,
						 mode, ret);
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
					path_get(&dir_path);
				}
				dir_path_retbuf = get_pathstr_from_path(&dir_path, &dir_path_tmpbuf);
				path_put(&dir_path);
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

			PRINT_TRACE(tpid->upid,
				    "!Open|fnamesize=%ld,forigsize=%ld,flags=%d,mode=%d,fd=%ld",
				    pathsize, final_orig_path_size, flags,
				    mode, ret);
			print_long_string(tpid->upid, retbuf, pathsize, "!FN",
					  current);
			print_long_string(tpid->upid, final_orig_path,
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

		if (!should_trace(&tpid))
			return;

		if (ret < 0) {
			break;
		}
		fd_arr = (int __user*) regs->di;
		err = get_user(fd1, fd_arr);
		if (err) {
			pr_warn("sys_exit_pipe: couldn't read fd1\n");
			break;
		}
		err = get_user(fd2, fd_arr + 1);
		if (err) {
			pr_warn("sys_exit_pipe: couldn't read fd2\n");
			break;
		}

		if (syscall_nr == __NR_pipe) {
			// from man pipe(2): If flags is 0, then pipe2() is the
			// same as pipe().
			flags = 0;
		} else {
			flags = (int) regs->si;
		}

		PRINT_TRACE(tpid->upid, "!Pipe|fd1=%d,fd2=%d,flags=%d",
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

		if (!should_trace(&tpid))
			return;

		if (ret < 0) {
			PRINT_TRACE(tpid->upid, "!RenameFailed|");
			break;
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
			break;
		}

		path_buf = get_pathstr_from_path(&to, &tmp_buf);
		path_put(&to);
		if (!path_buf) {
			break;
		}
		len = strlen(path_buf);

		PRINT_TRACE(tpid->upid, "!RenameTo|fnamesize=%ld", len);
		print_long_string(tpid->upid, path_buf, len, "!RT", current);
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

		if (!should_trace(&tpid))
			return;

		if (ret < 0) {
			PRINT_TRACE(tpid->upid, "!LinkFailed|");
			break;
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
			break;
		}

		path_buf = get_pathstr_from_path(&to, &tmp_buf);
		path_put(&to);
		if (!path_buf) {
			break;
		}
		len = strlen(path_buf);

		PRINT_TRACE(tpid->upid, "!LinkTo|fnamesize=%ld", len);
		print_long_string(tpid->upid, path_buf, len, "!LT", current);
		kfree(tmp_buf);

		break;
	}
	case __NR_symlink:
	case __NR_symlinkat: {
		struct path target, to;
		char __user *user_str, __user *user_target_str;
		char *path_buf, *tmp_buf = NULL, *resolved_target_tmp_buf = NULL,
		     *resolved_target_str, *target_str = NULL;
		int err, target_resolve_err, dir_fd;
		unsigned long len, target_len, resolved_target_len;
		bool target_resolved = false;

		if (!should_trace(&tpid))
			return;

		if (ret < 0) {
			break;
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
			break;
		}

		path_buf = get_pathstr_from_path(&to, &tmp_buf);
		path_put(&to);
		if (!path_buf)
			break;
		len = strlen(path_buf);

		user_target_str = (char __user *) regs->di;

		target_len = strnlen_user(user_target_str, 4096*32);
		if (!target_len) {
			pr_warn("sys_exit_symlink: strnlen_user failed\n");
			kfree(tmp_buf);
			break;
		}

		target_str = kmalloc(target_len, GFP_KERNEL);
		if (!target_str) {
			pr_warn("sys_exit_symlink: kalloc for target string failed\n");
			kfree(tmp_buf);
			break;
		}

		if (strncpy_from_user(target_str,
				      user_target_str, target_len) < 0) {
			pr_warn("sys_exit_symlink: strncpy_from_user failed\n");
			kfree(target_str);
			kfree(tmp_buf);
			break;
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
			path_put(&target);
			if (resolved_target_str) {
				resolved_target_len = strlen(resolved_target_str);
				target_resolved = true;
			}
		}

		if (target_resolved) {
			PRINT_TRACE(tpid->upid,
				    "!Symlink|targetnamesize=%ld,resolvednamesize=%ld,linknamesize=%ld",
				    target_len, resolved_target_len, len);
			print_long_string(tpid->upid, target_str, target_len,
					  "!ST", current);
			print_long_string(tpid->upid, resolved_target_str,
					  resolved_target_len, "!SR", current);
			print_long_string(tpid->upid, path_buf, len, "!SL",
					  current);

		} else {
			PRINT_TRACE(tpid->upid,
				    "!Symlink|targetnamesize=%ld,linknamesize=%ld",
				    target_len, len);
			print_long_string(tpid->upid, target_str, target_len,
					  "!ST", current);
			print_long_string(tpid->upid, path_buf, len, "!SL",
					  current);
		}

		kfree(resolved_target_tmp_buf);
		kfree(target_str);
		kfree(tmp_buf);
		break;
	}
	case __NR_dup:
	case __NR_dup2:
	case __NR_dup3: {
		int old_fd;
		int flags = 0;

		if (!should_trace(&tpid))
			return;

		if (ret < 0) {
			break;
		}

		if (syscall_nr == __NR_dup3) {
			flags = (int) regs->dx;
		}

		old_fd = (int) regs->di;
		PRINT_TRACE(tpid->upid, "!Dup|oldfd=%d,newfd=%d,flags=%d",
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

		if (!should_trace(&tpid))
			return;

		if (ret < 0) {
			break;
		}

		if (cmd == F_DUPFD_CLOEXEC) {
			flags = O_CLOEXEC;
		}

		old_fd = (int) regs->di;
		PRINT_TRACE(tpid->upid, "!Dup|oldfd=%d,newfd=%d,flags=%d",
			    old_fd, (int) ret, flags);
		break;
	}
	} // switch
out:
	release_traced_pid(tpid);
}

static void tracepoint_probe_fork(void* data, struct task_struct* self,
				  struct task_struct *task) {
	preempt_enable_notrace();
	__tracepoint_probe_fork(data, self, task);
	preempt_disable_notrace();
}

static void tracepoint_probe_exit(void* data, struct task_struct *task) {
	preempt_enable_notrace();
	__tracepoint_probe_exit(data, task);
	preempt_disable_notrace();
}

static void tracepoint_probe_exec(void* data, struct task_struct *p,
				  pid_t old_pid, struct linux_binprm *bprm)
{
	preempt_enable_notrace();
	__tracepoint_probe_exec(data, p, old_pid, bprm);
	preempt_disable_notrace();
}

static void tracepoint_probe_sys_enter(void* data, struct pt_regs *regs,
				       long id) {
	preempt_enable_notrace();
	__tracepoint_probe_sys_enter(data, regs, id);
	preempt_disable_notrace();
}

static void tracepoint_probe_sys_exit(void *data, struct pt_regs *regs,
				      long ret) {
	preempt_enable_notrace();
	__tracepoint_probe_sys_exit(data, regs, ret);
	preempt_disable_notrace();
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
	pr_info("BAS tracer [" TRACER_VERSION "]");
	pr_info("et_init called\n");

	if (ignore_repeated_opens) {
		pr_info("Repeated open() calls will be ignored");
	}

	// check if bits_of_real_pid is sane

	if (bits_of_real_pid < 0 || bits_of_real_pid > 32) {
		pr_err("Invalid bits_of_real_pid value. Acceptable values are [0, 32].");
		return -EINVAL;
	}

	if (support_ns_pid) {
		pr_info("Trying to support namespace pid");
	}

	// read root pid from parameter

	if (!root_pid) {
		pr_err("Invalid root_pid value (pass root_pid=<pid> when insmod'ing)\n");
		return -EINVAL;
	}
	maybe_add_pid_to_list((pid_t) root_pid, (pid_t) 0/* don't care */, (pid_t) 0/* don't care */, true);

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
