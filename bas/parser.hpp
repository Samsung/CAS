#pragma once

#include <climits>
#include <cstring>

#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <optional>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "error.hpp"
#include "tags.hpp"

/* Generated using 'gperf phash.txt > phash.h' */
#include "phash.h"

#include "BS_thread_pool.hpp"

constexpr ssize_t NOT_INDEXED = -1;

struct fdinfo;
struct ParsingStatistics;
class StreamParser;
class CachingParser;

typedef int64_t upid_t;
typedef std::map<int, fdinfo> fdmap_t;
typedef std::pair<std::pair<upid_t, unsigned>, std::set<std::pair<upid_t, unsigned>>> pipe_map_pair_t;
typedef std::map<std::pair<upid_t, unsigned>, std::set<std::pair<upid_t, unsigned>>> pipe_map_t;

#undef ENABLE_PARENT_PIPE_CHECK
#define ENABLE_SIBLING_PIPE_CHECK
#define SPLIT_EXEC_ENTRY_AT_SIZE 0
#define EVENT_FLUSH_CACHE_MARGIN \
    10000 /* How many events to cache before flushing all events for a given process after receiving 'Exit' */

template <typename... Args>
inline void format_print(Args &&... arg) {
    std::cerr << "err: ";

    ([&]() {
        std::cerr << arg;
    } (), ...);

    std::cerr << '\n';
}

struct NewProcArguments {
    size_t argsize;
    size_t prognameisize;
    size_t prognamepsize;
    size_t cwdsize;
};

struct SchedForkArguments {
    upid_t cpid;
};

struct CloseArguments {
    int fd;
};

struct SysCloneArguments {
    unsigned long flags;
};

struct OpenArguments {
    unsigned long fnamesize;
    unsigned long forigsize;
    unsigned long flags;
    long mode;
    int fd;
};

struct PipeArguments {
    int fd1;
    int fd2;
    unsigned long flags;
};

struct DupArguments {
    int oldfd;
    int newfd;
    unsigned long flags;
};

struct RenameArguments {
    unsigned long fnamesize;
    unsigned long flags;
};

struct LinkArguments {
    unsigned long fnamesize;
    unsigned long flags;
};

struct SymlinkArguments {
    unsigned long targetnamesize;
    ssize_t resolvednamesize;
    unsigned long linknamesize;
};

struct ExitArguments {
    char status;
};

typedef struct eventTuple {
    upid_t pid;
    unsigned cpu;
    unsigned long timestamp;
    size_t line_number;
    Tag tag;
    ssize_t argument_index;
    std::string event_arguments;
} eventTuple_t;

struct OpenFile {
    OpenFile(void) = default;
    OpenFile(const OpenFile &) = default;
    OpenFile(OpenFile &&) = default;
    OpenFile& operator=(const OpenFile &) = default;
    OpenFile& operator=(OpenFile &&) = default;

    std::string absolute_path;
    size_t size;
    int mode;

    void resolve_runtime_variables(std::string &original_path) {
        int ret;
        struct statx stat_buf;

        this->size = 0;

        ret = statx(-1, original_path.c_str(), AT_SYMLINK_NOFOLLOW,
                    STATX_TYPE | STATX_SIZE, &stat_buf);

        if (ret)
            return;

        this->size = stat_buf.stx_size;

        /*
         * @NOTE: Since it is pretty confusing, here is another description for our mode format:
         * 0bEMMMMAA
         *
         * E bit is set when the file actually exists, otherwise the rest doesn't matter. To not cause
         * confusion, we set it all to zero.
         *
         * M bits are are file type bits taken from the stat() syscall.
         *
         * A bits are access mode bits, used in the open() syscall.
         */
        this->mode |= 0x40 | ((stat_buf.stx_mode) >> 10);
    };
};

struct Child {
    upid_t pid;
    int flags;

    Child(upid_t pid, int flags)
        : pid (pid)
        , flags (flags)
    {};
};


struct Execution {
    Execution() = default;
    Execution(upid_t pid)
        : pid (pid)
    {};

    upid_t pid;
    unsigned index = 0;
    uint64_t elapsed_time;

    std::map<std::string, OpenFile> opened_files;
    std::pair<upid_t, unsigned> parent;
    std::string program_path;
    std::string current_working_directory;
    std::vector<std::string> arguments;
    std::vector<Child> children;
    uint8_t exit_code = 0;

    void add_open_file(std::string &path, OpenFile &file) {
        auto lookup = opened_files.find(path);
        if (lookup == opened_files.end()) {
            file.resolve_runtime_variables(path);
            opened_files.insert_or_assign(std::move(path), std::move(file));
        } else if (lookup != opened_files.end() && (lookup->second.mode & 0x03) != (file.mode & 0x03)) {
            auto &registered_file = lookup->second;
            registered_file.mode = O_RDWR;
            registered_file.resolve_runtime_variables(path);
        }
    }
};


class Process {
    friend StreamParser;
    friend CachingParser;
private:
    std::vector<eventTuple_t> event_list;
public:
    upid_t pid;

    uint64_t first_event_time;
    uint64_t last_event_time;

    std::vector<Execution> executions;

    Process(upid_t pid, eventTuple_t &event)
        : pid (pid)
        , first_event_time (0)
        , last_event_time (0)
    {
        event_list.push_back(event);
        executions.push_back(Execution(pid));
    }
};

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
    syscall_raw(upid_t pid, enum sysname sn, int i0, int i1, unsigned long ul, uint64_t st)
        : sysname(sn), pid(pid), pv(-1), i0(i0), i1(i1), ul(ul), start_time(st) {
    }
    syscall_raw(upid_t pid, enum sysname sn, upid_t pv, unsigned long ul, uint64_t st)
        : sysname(sn), pid(pid), pv(pv), i0(-1), i1(-1), ul(ul), start_time(st) {
    }
    syscall_raw(upid_t pid, enum sysname sn, uint64_t st)
        : sysname(sn), pid(pid), pv(-1), i0(-1), i1(-1), ul(0), start_time(st) {
    }
};

struct CacheEntry {
    upid_t pid;
    unsigned long hitcount;

    CacheEntry(upid_t pid, uint64_t hitcount)
        : pid (pid)
        , hitcount (hitcount)
    {};
};

/*
 * @NOTE: Some of these can be resolved after parsing, like total execs or total events, therefore
 * reducing the amount of stuff we need to count. But so far so good, it does not matter that much
 * because std::memory_order_relaxed add a minimal amount of overhead to atomic memory accesses.
 */
class StatsCollector {
    friend ParsingStatistics;
private:
    std::atomic<size_t> exec_count;
    std::atomic<size_t> fork_count;
    std::atomic<size_t> close_count;
    std::atomic<size_t> open_count;
    std::atomic<size_t> pipe_count;
    std::atomic<size_t> dup_count;
    std::atomic<size_t> rename_count;
    std::atomic<size_t> symlink_count;
    std::atomic<size_t> link_count;
    std::atomic<size_t> exit_count;
    std::atomic<size_t> total_event_count;
    std::atomic<size_t> total_execs_count;
    std::atomic<size_t> process_count;
    std::atomic<size_t> procs_at_exit;
    std::atomic<size_t> multilines_count;
public:
    StatsCollector() = default;

    void increment_exec(void) {
        exec_count.fetch_add(1, std::memory_order_relaxed);
    };

    void increment_fork(void) {
        fork_count.fetch_add(1, std::memory_order_relaxed);
    };

    void increment_close(void) {
        close_count.fetch_add(1, std::memory_order_relaxed);
    };

    void increment_open(void) {
        open_count.fetch_add(1, std::memory_order_relaxed);
    };

    void increment_pipe(void) {
        pipe_count.fetch_add(1, std::memory_order_relaxed);
    };

    void increment_dup(void) {
        dup_count.fetch_add(1, std::memory_order_relaxed);
    };

    void increment_rename(void) {
        rename_count.fetch_add(1, std::memory_order_relaxed);
    };

    void increment_symlink(void) {
        symlink_count.fetch_add(1, std::memory_order_relaxed);
    };

    void increment_link(void) {
        link_count.fetch_add(1, std::memory_order_relaxed);
    };

    void increment_exit(void) {
        exit_count.fetch_add(1, std::memory_order_relaxed);
    };

    void increment_processes(void) {
        process_count.fetch_add(1, std::memory_order_relaxed);
    };

    void increment_procs_at_exit(void) {
        procs_at_exit.fetch_add(1, std::memory_order_relaxed);
    };

    void increment_multilines(void) {
        multilines_count.fetch_add(1, std::memory_order_relaxed);
    };

    void add_processed(size_t processed) {
        total_event_count.fetch_add(processed, std::memory_order_relaxed);
    };

    void add_execs(size_t processed) {
        total_execs_count.fetch_add(processed, std::memory_order_relaxed);
    };
};

struct ParsingStatistics {
    ParsingStatistics(void)
        : exec_count (0)
        , fork_count (0)
        , close_count (0)
        , open_count (0)
        , pipe_count (0)
        , dup_count (0)
        , rename_count (0)
        , link_count (0)
        , symlink_count (0)
        , exit_count (0)
        , multilines_count (0)
    {};

    explicit ParsingStatistics(StatsCollector& collector)
        : exec_count (collector.exec_count.load())
        , fork_count (collector.fork_count.load())
        , close_count (collector.close_count.load())
        , open_count (collector.open_count.load())
        , pipe_count (collector.pipe_count.load())
        , dup_count (collector.dup_count.load())
        , rename_count (collector.dup_count.load())
        , link_count (collector.link_count.load())
        , symlink_count (collector.symlink_count.load())
        , exit_count (collector.exit_count.load())
        , total_event_count (collector.total_event_count.load())
        , total_execs_count (collector.total_execs_count.load())
        , process_count (collector.process_count.load())
        , procs_at_exit (collector.procs_at_exit.load())
        , multilines_count (collector.multilines_count.load())
    {};

    size_t exec_count;
    size_t fork_count;
    size_t close_count;
    size_t open_count;
    size_t pipe_count;
    size_t dup_count;
    size_t rename_count;
    size_t link_count;
    size_t symlink_count;
    size_t exit_count;
    size_t total_event_count;
    size_t total_execs_count;
    size_t process_count;
    size_t procs_at_exit;
    size_t multilines_count;
    size_t empty_child_count;
};

struct ParsingResults {
    std::map<upid_t, Process> process_map;
    std::map<upid_t, std::pair<upid_t, unsigned>> parent_map;
    std::vector<syscall_raw> syscalls;

    ParsingResults(std::map<upid_t, Process> &&process_map,
            std::map<upid_t, std::pair<upid_t, unsigned>> &&parent_map,
            std::vector<syscall_raw> &&syscalls)
        : process_map (std::move(process_map))
        , parent_map (std::move(parent_map))
        , syscalls (std::move(syscalls))
    {};
};

class StreamParser {
private:
    uint64_t m_last_event_time;
    uint64_t m_first_event_time;
    StatsCollector m_stats_collector;
    std::map<upid_t, Process> m_process_map;
    std::map<upid_t, std::pair<upid_t, unsigned>> m_parent_map;
    std::vector<syscall_raw> m_syscalls;

protected:
    static Errorable<NewProcArguments> parse_newproc_short_arguments(const eventTuple_t &);
    static Errorable<SchedForkArguments> parse_schedfork_short_arguments(const eventTuple_t &);
    static Errorable<SysCloneArguments> parse_sysclone_short_arguments(const eventTuple_t &);
    static Errorable<OpenArguments> parse_open_short_arguments(const eventTuple_t &);
    static Errorable<CloseArguments> parse_close_short_arguments(const eventTuple_t &);
    static Errorable<PipeArguments> parse_pipe_short_arguments(const eventTuple_t &);
    static Errorable<RenameArguments> parse_rename_short_arguments(const eventTuple_t &);
    static Errorable<LinkArguments> parse_link_short_arguments(const eventTuple_t &);
    static Errorable<ExitArguments> parse_exit_short_arguments(const eventTuple_t &);
    static Errorable<RenameArguments> parse_rename2_short_arguments(const eventTuple_t &);
    static Errorable<LinkArguments> parse_linkat_short_arguments(const eventTuple_t &);
    static Errorable<DupArguments> parse_dup_short_arguments(const eventTuple_t &);
    static Errorable<SymlinkArguments> parse_symlink_short_arguments(const eventTuple_t &);
    static size_t parse_arguments(std::vector<eventTuple_t>::iterator &,
                            const std::vector<eventTuple_t>::iterator &,
                            std::vector<std::string> &);
    static size_t parse_long_argument(std::vector<eventTuple_t>::iterator &it,
            const std::vector<eventTuple_t>::iterator &, Tag, Tag, Tag, std::string &);
    static Errorable<eventTuple_t> parse_generic_args(const char *, uint64_t);

    virtual std::vector<CacheEntry>& cache(void) = 0;

    virtual void append_syscall(syscall_raw &&) = 0;
    virtual void register_child(upid_t, upid_t, size_t) = 0;
    virtual void schedule_processing(eventTuple_t &) = 0;

    Errorable<void> process_events(Process &);

    StatsCollector& stats_collector(void) {
        return m_stats_collector;
    };

    void set_last_event_time(uint64_t timestamp) {
        m_last_event_time = timestamp;
    };

    uint64_t last_event_time(void) {
        return m_last_event_time;
    };

    void set_first_event_time(uint64_t timestamp) {
        m_first_event_time = timestamp;
    };

    uint64_t first_event_time(void) {
        return m_first_event_time;
    };

    std::map<upid_t, Process>& process_map(void) {
        return m_process_map;
    };

    std::vector<syscall_raw>& syscalls(void) {
        return m_syscalls;
    };

    std::map<upid_t, std::pair<upid_t, unsigned>>& parent_map(void) {
        return m_parent_map;
    };


public:
    virtual ~StreamParser() = default;

    ParsingResults release_results(void) {
        return ParsingResults(std::move(m_process_map),
                std::move(m_parent_map), std::move(m_syscalls));
    };

    virtual size_t cache_lifetime(void) = 0;
    virtual void set_cache_lifetime(size_t) = 0;

    virtual void finish_parsing(void) = 0;

    ParsingStatistics stats(void) {
        auto& _parent_map = parent_map();
        auto& _process_map = process_map();
        ParsingStatistics stats = ParsingStatistics(m_stats_collector);

        for (auto i = _parent_map.begin(), end = _parent_map.end(); i != end; ++i) {
            auto process_map_end = _process_map.end();
            if (_process_map.find(i->first) == process_map_end)
                stats.empty_child_count++;
        }

        return stats;
    };

    Errorable<void> parse_line(const char *, size_t, uint64_t);
};

class CachingParser : public StreamParser {
private:
    std::vector<CacheEntry> m_cache;
    std::set<upid_t> m_pending_exits;
    size_t m_cache_lifetime = EVENT_FLUSH_CACHE_MARGIN;

protected:
    std::vector<CacheEntry> &cache(void) final override {
        return m_cache;
    };

    virtual void trigger_parsing(Process &) = 0;

    std::set<upid_t>& pending_exits(void) {
        return m_pending_exits;
    };

    void schedule_processing(eventTuple_t &) final override;

    void cache_hit(Process &, bool pending = false);
    void cleanup_cache(void);
public:
    size_t cache_lifetime(void) final override {
        return m_cache_lifetime;
    };

    void set_cache_lifetime(size_t cache_lifetime) final override {
        m_cache_lifetime = cache_lifetime;
    };

    virtual void finish_parsing(void) override {
        cleanup_cache();
    };
};

class SinglethreadedParser final : public CachingParser {
private:
    void trigger_parsing(Process &process) final override {
        auto result = process_events(process);
        if (result.is_error())
            std::cout << result.explain();
    };

    void append_syscall(syscall_raw &&syscall) final override {
        auto& _syscalls = syscalls();
        _syscalls.push_back(syscall);
    };

    void register_child(upid_t child, upid_t parent, size_t execution) final override {
        auto& _parent_map = parent_map();
        _parent_map.insert_or_assign(child, std::make_pair(parent, execution));
    };

public:
    SinglethreadedParser(void) = default;
    ~SinglethreadedParser() = default;

    virtual void finish_parsing(void) final override {
        CachingParser::finish_parsing();
    };
};

class MultithreadedParser final : public CachingParser {
private:
    BS::thread_pool m_pool;

    std::mutex m_parent_map_mutex;
    std::mutex m_syscalls_mutex;

    void trigger_parsing(Process &process) final override {
        m_pool.push_task([this, &process]() mutable {
            auto result = process_events(process);
            if (result.is_error())
                std::cout << result.explain();
        });
    };

    void append_syscall(syscall_raw &&syscall) final override {
        std::unique_lock lock(m_syscalls_mutex);
        auto& _syscalls = syscalls();
        _syscalls.push_back(syscall);
    };

    void register_child(upid_t child, upid_t parent, size_t execution) final override {
        std::unique_lock lock(m_parent_map_mutex);
        auto& _parent_map = parent_map();
        _parent_map.insert(std::make_pair(child, std::make_pair(parent, execution)));
    };

public:
    MultithreadedParser(void) = default;
    MultithreadedParser(size_t threads)
        : m_pool (threads)
        {};

    ~MultithreadedParser() {
        m_pool.wait_for_tasks();
    };

    virtual void finish_parsing(void) final override {
        CachingParser::finish_parsing();
        m_pool.wait_for_tasks();
    };
};

struct fdinfo {
    fdinfo() : piperd(0), pipewr(0), cloexec(0), pipe_mark(0) {
    }
    fdinfo(uint32_t rd, uint32_t wr, uint32_t cloexec, uint32_t pipe_mark)
        : piperd(rd), pipewr(wr), cloexec(cloexec), pipe_mark(pipe_mark) {
    }
    uint32_t piperd : 1;     // read end of a pipe produced by the PIPE like syscall with the given unique index
    uint32_t pipewr : 1;     // write end of a pipe produced by the PIPE like syscall with the given unique index
    uint32_t cloexec : 1;    // non-zero when this pipe file descriptor has CLOEXEC flag set
    uint32_t pipe_mark : 30; // unique index for the specific PIPE like syscall
};

struct fdmap_node {
    fdmap_node() {
    }
    fdmap_node(upid_t pid) {
        refs.insert(pid);
    }
    fdmap_t fdmap;
    std::set<upid_t> refs; /* collection of processes this file descriptor is shared with */
};

static inline void update_pipe_map(pipe_map_t &pipe_map, upid_t kpid, unsigned kexeIdx, upid_t vpid, unsigned vexeIdx) {

    std::pair<upid_t, unsigned> k(kpid, kexeIdx);
    std::pair<upid_t, unsigned> v(vpid, vexeIdx);

    if (pipe_map.find(k) != pipe_map.end()) {
        pipe_map[k].insert(v);
    } else {
        pipe_map.insert(pipe_map_pair_t(k, std::set<std::pair<upid_t, unsigned>>()));
        pipe_map[k].insert(v);
    }
}
