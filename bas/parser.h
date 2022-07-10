#ifndef __PARSER_H__
#define __PARSER_H__

#include "utils.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef int64_t upid_t;

#define GENERIC_ARG_PID_FMT "%" PRId64
#define GENERIC_ARG_CPU_FMT "%u"
#define GENERIC_ARG_TIME_FMT "%lu"
#define GENERIC_ARG_TIMEN_FMT "%lu"
#define SYSEVENT_SCHEDFORK_ARG_PID_FMT	"%" PRId64
#define SYSEVENT_NEWPROC_ARG_ARGSIZE_FMT	"%lu"
#define SYSEVENT_NEWPROC_ARG_PROGNAMEISIZE_FMT	"%lu"
#define SYSEVENT_NEWPROC_ARG_PROGNAMEPSIZE_FMT	"%lu"
#define SYSEVENT_NEWPROC_ARG_CWDSIZE_FMT	"%lu"
#define SYSEVENT__ARG_FD_FMT	"%u"
#define SYSEVENT__ARG_FLAGS_FMT	"%lu"
#define SYSEVENT_OPEN_ARG_MODE_FMT	"%ld"
#define SYSEVENT__ARG_FNAMESIZE_FMT	"%lu"
#define SYSEVENT_SYMLINK_ARG_TARGETNAMESIZE_FMT	"%lu"
#define SYSEVENT_SYMLINK_ARG_LINKNAMESIZE_FMT	"%lu"
#define SYSEVENT_SYMLINK_ARG_RESOLVEDNAMESIZE_FMT	"%lu"

#undef ENABLE_PARENT_PIPE_CHECK
#define ENABLE_SIBLING_PIPE_CHECK
#define SPLIT_EXEC_ENTRY_AT_SIZE 0
#define EVENT_FLUSH_CACHE_MARGIN 10000 /* How many events to cache before flushing all events for a given process after receiving 'Exit' */

/* Perfect hash values for selected strings */

#define PHASH_New_proc 53
#define PHASH_SchedFork 9
#define PHASH_SysClone 8
#define PHASH_Close 10
#define PHASH_Pipe 44
#define PHASH_Open 54
#define PHASH_RenameFrom 25
#define PHASH_Rename2From 36
#define PHASH_LinkFrom 13
#define PHASH_LinkatFrom 15
#define PHASH_Symlink 7
#define PHASH_RenameTo 23
#define PHASH_RenameFailed 47
#define PHASH_LinkTo 11
#define PHASH_LinkFailed 30
#define PHASH_Exit 49
#define PHASH_pid 28
#define PHASH_time 4
#define PHASH_timen 5
#define PHASH_argsize 27
#define PHASH_ppid 34
#define PHASH_fd 12
#define PHASH_flags 35
#define PHASH_mode 19
#define PHASH_fnamesize 39
#define PHASH_forigsize 29
#define PHASH_targetnamesize 24
#define PHASH_linknamesize 37
#define PHASH_resolvednamesize 51
#define PHASH_Progname_i 60
#define PHASH_Progname_p 50
#define PHASH_End_of_args 61
#define PHASH_fd1 68
#define PHASH_fd2 63
#define PHASH_cpu 48
#define PHASH_cwd 38
#define PHASH_prognameisize 43
#define PHASH_prognamepsize 33
#define PHASH_cwdsize 42
#define PHASH_Cont 14
#define PHASH_Cont_end 18
#define PHASH_SysCloneFailed 59
#define PHASH_Dup 58
#define PHASH_oldfd 20
#define PHASH_newfd 40

#define PHASH_STR(s)	PHASH_##s

#define CLONE_FILES     0x00000400

enum SYSEVENT {
	SYSEVENT_NEWPROC,
	SYSEVENT_SCHEDFORK,
	SYSEVENT_SYSCLONE,
	SYSEVENT_CLOSE,
	SYSEVENT_PIPE,
	SYSEVENT_DUP,
	SYSEVENT_OPEN,
	SYSEVENT_RENAMEFROM,
	SYSEVENT_RENAME2FROM,
	SYSEVENT_RENAMETO,
	SYSEVENT_LINKFROM,
	SYSEVENT_LINKATFROM,
	SYSEVENT_LINKTO,
	SYSEVENT_SYMLINK,
	SYSEVENT_EXIT,
};

typedef std::pair<std::pair<upid_t,unsigned>,std::set<std::pair<upid_t,unsigned>>> pipe_map_pair_t;
typedef std::map<std::pair<upid_t,unsigned>,std::set<std::pair<upid_t,unsigned>>> pipe_map_t;

struct event_count {
	long exec;
	long fork;
	long close;
	long open;
	long pipe;
	long dup;
	long rename;
	long link;
	long symlink;
	long exit;
};

struct parsed_entry {
	parsed_entry(): pid(0), procidx(0), valid(false), parent(-1,0), etime(0) {}
	parsed_entry(upid_t pid): pid(pid), procidx(0), valid(false), parent(-1,0), etime(0) {}
	parsed_entry(upid_t pid, unsigned procidx, const char* progname_p, const char* cwd):
		pid(pid), procidx(procidx), valid(true), progname_p(progname_p), cwd(cwd), parent(-1,0), etime(0) {}
	upid_t pid;
	unsigned procidx;
	bool valid;
	std::string progname_p;
	std::string cwd;
	std::vector<std::string> argv;
	std::vector<std::pair<upid_t,unsigned long>> vchild;
	std::map<std::pair<std::string,std::string>,std::pair<unsigned,long>> rwmap;
	std::pair<upid_t,unsigned> parent;
	uint64_t etime;
	void addUpdateFile(const char* p, unsigned flags, upid_t pid=-1, const char* o = 0) {
		std::pair<std::string,std::string> k;
		k.first = p;
		const char* original_path = p;
		if (o && strcmp(p,o)) {
			k.second = o;
			original_path = o;
		}
		if (this->rwmap.find(k)!=this->rwmap.end()) {
			/* If both are the same the value is unchanged, otherwise it gets O_RDWR */
			if ((this->rwmap[k].first&0x03)!=(flags&0x3)) {
				this->rwmap[k].first = O_RDWR;
			}
		}
		else {
			this->rwmap.insert(std::pair<std::pair<std::string,std::string>,std::pair<unsigned,long>>(k,
					std::pair<unsigned,unsigned>(flags&0x3,0)));
		}
		if (access(k.first.c_str(),F_OK)!=-1) {
			struct stat path_stat;
			lstat(original_path, &path_stat);
			/* bits: 0EMMMM00 where E is 1 (file exists) and MMMM are 4 bits of the stat mode parameter */
			unsigned mode = 0x40 | ((path_stat.st_mode&0170000)>>10);
			this->rwmap[k].first |= mode;
			this->rwmap[k].second = path_stat.st_size;
		}
	}
	void settime(uint64_t etime) {
		this->etime = etime;
	}
};

static inline uint64_t make_uint64t(uint32_t a, uint32_t b) {
	return (((uint64_t)a)<<32)|((uint64_t)b);
}

struct parsed_entry_sort_key
{
    inline bool operator() (const parsed_entry& e1, const parsed_entry& e2)
    {
    	if (e1.pid<e2.pid) return true;
    	if (e1.pid>e2.pid) return false;
    	return e1.procidx < e2.procidx;
    }
};

struct syscall_raw {
	enum sysname {
		SYS_PIPE,
		SYS_DUP,
		SYS_FORK,
		SYS_CLOSE,
		SYS_EXEC,
		SYS_EXIT,
	} sysname;
	upid_t pid;
	upid_t pv;
	int i0;
	int i1;
	unsigned long ul;
	uint64_t start_time;
	syscall_raw(upid_t pid, enum sysname sn, int i0, int i1, unsigned long ul, uint64_t st):
		sysname(sn), pid(pid), pv(-1), i0(i0), i1(i1), ul(ul), start_time(st) {}
	syscall_raw(upid_t pid, enum sysname sn, upid_t pv, unsigned long ul, uint64_t st):
		sysname(sn), pid(pid), pv(pv), i0(-1), i1(-1), ul(0), start_time(st) {}
	syscall_raw(upid_t pid, enum sysname sn, uint64_t st):
		sysname(sn), pid(pid), pv(-1), i0(-1), i1(-1), ul(0), start_time(st) {}
};

struct syscall_raw_sort_key
{
    inline bool operator() (const syscall_raw& e1, const syscall_raw& e2)
    {
    	return e1.start_time < e2.start_time;
    }
};

struct parse_context {
	FILE* rawoutfd;
	FILE* outfd;
	long total_event_count;
	long total_rw_count;
	long total_fork_count;
	long total_execs_count;
	long process_count;
	uint64_t start_time;
	upid_t root_pid;
	struct event_count event_count;
	std::map<upid_t,std::pair<upid_t,unsigned>> rev_fork_map;
	std::vector<parsed_entry> ve;
	std::vector<syscall_raw> srvec;
};

struct fdinfo {
	fdinfo(): piperd(0), pipewr(0), cloexec(0), pipe_mark(0) {}
	fdinfo(uint32_t rd, uint32_t wr, uint32_t cloexec, uint32_t pipe_mark):
		piperd(rd), pipewr(wr), cloexec(cloexec), pipe_mark(pipe_mark) {}
	uint32_t piperd: 1;      // read end of a pipe produced by the PIPE like syscall with the given unique index
	uint32_t pipewr: 1;      // write end of a pipe produced by the PIPE like syscall with the given unique index
	uint32_t cloexec: 1;     // non-zero when this pipe file descriptor has CLOEXEC flag set
	uint32_t pipe_mark: 30;  // unique index for the specific PIPE like syscall
};

typedef std::map<int,fdinfo> fdmap_t;

struct fdmap_node {
	fdmap_node() {}
	fdmap_node(upid_t pid) { refs.insert(pid); }
	fdmap_t fdmap;
	std::set<upid_t> refs; /* collection of processes this file descriptor is shared with */
};

static inline void update_pipe_map(pipe_map_t& pipe_map, upid_t kpid, unsigned kexeIdx, upid_t vpid, unsigned vexeIdx) {

	std::pair<upid_t,unsigned> k(kpid,kexeIdx);
	std::pair<upid_t,unsigned> v(vpid,vexeIdx);

	if (pipe_map.find(k)!=pipe_map.end()) {
		pipe_map[k].insert(v);
	}
	else {
		pipe_map.insert(pipe_map_pair_t(k,std::set<std::pair<upid_t,unsigned>>()));
		pipe_map[k].insert(v);
	}
}

struct NewProcArgs {
	unsigned long argsize;
	unsigned long prognameisize;
	unsigned long prognamepsize;
	unsigned long cwdsize;
	const char* progname_i;
	const char* progname_p;
	const char* cwd;
	vs_t argv;
};

struct SchedForkArgs {
	upid_t cpid;
};

struct CloseArgs {
	int fd;
};

struct SysCloneArgs {
	unsigned long flags;
};

struct OpenArgs {
	unsigned long fnamesize;
	unsigned long forigsize;
	unsigned long flags;
	long mode;
	int fd;
};

struct PipeArgs {
	int fd1;
	int fd2;
	unsigned long flags;
};

struct DupArgs {
	int oldfd;
	int newfd;
	unsigned long flags;
};

struct RenameArgs {
	unsigned long fnamesize;
	unsigned long flags;
};

struct LinkArgs {
	unsigned long fnamesize;
	unsigned long flags;
};

struct SymLinkArgs {
	unsigned long targetnamesize;
	unsigned long resolvednamesize;
	unsigned long linknamesize;
};

struct ExitArgs {
};

typedef struct eventTuple {
	upid_t pid;
	unsigned cpu;
	unsigned long time;
	unsigned long timen;
    char* event_line;
    uint8_t event;
} eventTuple_t;

typedef eventTuple_t* peventTuple_t;
typedef VEC_DECL(eventTuple_t*) vpeventTuple_t;
typedef vpeventTuple_t* pvpeventTuple_t;

#if defined __cplusplus
extern "C" {
#endif
	int parse_generic_args(const char* line, eventTuple_t* evln);
	long parse_write_process_events(upid_t pid, vpeventTuple_t* event_list, struct parse_context* context,
			uint64_t start_time, uint64_t end_time);
#if defined __cplusplus
}
#endif

#endif /* __PARSER_H__ */
