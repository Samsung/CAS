#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cerrno>
#include <csignal>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "parser.hpp"

static volatile int interrupt = 0;
static void intHandler(int signum) {
    (void) signum;

    interrupt = 1;
}

#ifdef LIBRARY_BUILD
extern "C" {
int parser_main(int argc, char **argv);
}
#endif

static int pipe_open_for_write(struct fdmap_node *from, struct fdmap_node *to, int stdpipe = 0) {
    std::set<uint32_t> pms;
    for (auto u = from->fdmap.begin(); u != from->fdmap.end(); ++u) {
        /* if stdpipe is 0 we don't care about the fd value */
        if (((*u).second.pipewr) && (((*u).first == 1) || (!stdpipe))) {
            pms.insert((*u).second.pipe_mark);
        }
    }
    for (auto u = to->fdmap.begin(); u != to->fdmap.end(); ++u) {
        if (((*u).second.piperd) && (((*u).first == 0) || (!stdpipe))) {
            if (pms.find((*u).second.pipe_mark) != pms.end()) {
                return 1;
            }
        }
    }
    return 0;
}

void flush_entries(ParsingResults& results, pipe_map_t& pipe_map, std::ostream& output, size_t split = 0) {
    size_t progress_counter = 0;
    size_t process_map_size = results.process_map.size();

    std::cout << "Flushing entries...\n";
    std::cout.flush();

    output << "[\n";

    auto print_entry = [&](Execution& execution) {
        if (results.parent_map.find(execution.pid) != results.parent_map.end())
            execution.parent = results.parent_map[execution.pid];
        else
            execution.parent = std::pair<upid_t, unsigned>(-1, 0);

        output << "{\"p\":" << execution.pid;
        output << ",\"x\":" << execution.index;
        output << ",\"e\":" << execution.elapsed_time;
        output << ",\"b\":\"" << execution.program_path << "\"";
        output << ",\"w\":\"" << execution.current_working_directory << "\"";
        output << ",\"v\":[";

        for (auto u = execution.arguments.begin(); u != execution.arguments.end(); ++u) {
            output << "\"" << *u << "\"";
            if (u != execution.arguments.end() - 1)
                output << ',';
        }

        output << "],\"c\":[";

        for (auto u = execution.children.begin(); u != execution.children.end(); ++u) {
            auto &child = *u;
            output << "{\"p\":" << child.pid << ",\"f\":" << child.flags << "}";
            if (u != execution.children.end() - 1)
                output << ',';
        }

        output << "],\"r\":{\"p\":" << execution.parent.first << ",\"x\":" << execution.parent.second << "},";
        output << "\"i\":[";

        std::pair<upid_t, unsigned> pipe_map_key(execution.pid, execution.index);
        if (pipe_map.find(pipe_map_key) != pipe_map.end()) {
            std::set<std::pair<upid_t, unsigned>> &pmv = pipe_map[pipe_map_key];
            for (auto u = pmv.begin(); u != pmv.end(); ++u) {
                if (u != pmv.begin())
                    output << ',';

                output << "{\"p\":" << (*u).first << ",\"x\":" << (*u).second << '}';
            }
        }

        output << "]";
    };

    auto print_files = [&](Execution& execution) {
        size_t path_size = 0;

        output << ",\"o\":[";

        for (auto iter = execution.opened_files.cbegin(), end_iter = execution.opened_files.cend();
                iter != end_iter; ++iter) {
            auto &original_path = iter->first;
            auto &file = iter->second;

            output << "{\"p\":\"" << file.absolute_path << "\",";
            path_size += file.absolute_path.size();

            if (original_path != file.absolute_path) {
                output << "\"o\":\"" << original_path << "\",";
                path_size += original_path.size();
            }

            output << "\"m\":" << file.mode << ",\"s\":" << file.size << '}';

            if (split && path_size >= split && std::next(iter) != end_iter) {
                output << "]},\n";
                print_entry(execution);
                output << ",\"o\":[";
                path_size = 0;
            }

            if (std::next(iter) != end_iter)
                output << ',';
        }

        output << "]}";
    };

    /* Now update parent pid information in parsed entries and flush the entries */
    for (auto it = results.process_map.begin(), end_iter = results.process_map.end();
            it != end_iter; ++it) {
        progress_counter++;
        if (isatty(STDOUT_FILENO) && !(progress_counter % 100)) {
            std::cout << (progress_counter * 100) / process_map_size << "%\r";
            std::cout.flush();
        }

        auto& process = it->second;
        for (size_t i = 0; i < process.executions.size(); i++) {
            auto& execution = process.executions[i];

            print_entry(execution);
            print_files(execution);

            if (std::next(it) != results.process_map.end()
                || (std::next(it) == results.process_map.end() && i != process.executions.size() - 1))
            {
                output << ",\n";
            }
        }

        if (interrupt)
            break;
    }

    output << "\n]";
}

void print_help() {
    std::cout << "Usage: etrace_parser [-t] [-j <N>] [-c <N>] [-s <N>] <path to tracer output> <destination of outputted file>\n";
    std::cout << "where:\n";
    std::cout << "\t<path to tracer output>: by default it is '.nfsdb' file in the current directory\n";
    std::cout << "\t<destination of outputted file>: by default it is '.nfsdb.json' file in the current directory\n";
    std::cout << "options:\n";
    std::cout << "\t-j <N>: amount of threads to create in the threadpool. Specifying 0 means that ";
    std::cout << "the parse will utilize std::thread::hardware_concurrency() - 1 threads. Defaults to 1\n";
    std::cout << "\t-c <N>: specify the cache margin. Specifying 0 means that the cache is disabled. Defaults to 10000.\n";
    std::cout << "\t-s <N>: specify amount of bytes at which the entry should be split, Specifying 0";
    std::cout << " means that the entries won't be split. This is especially useful when using versions";
    std::cout << " MongoDB that limit the single document size to 16M. Defaults to 0.\n";
    std::cout << "\t-t: print parsing statistics\n";
}

int parser_main(int argc, char **argv) {
    std::unique_ptr<StreamParser> parser;
    std::error_code err;
    std::ifstream input;
    std::ofstream output;

    struct sigaction act;
    size_t thread_count = 1;
    size_t entry_split = 0;
    size_t cache_lifetime = 10000;
    size_t file_size;
    size_t total_read = 0;
    unsigned long line_count = 0;
    const char *input_path = nullptr;
    const char *output_path = nullptr;
    char line[2048] = { 0 };
    char *endptr = nullptr;
    bool no_progress = false;
    bool print_stats = false;

    act.sa_handler = intHandler;
    sigaction(SIGINT, &act, 0);
    interrupt = 0;

    for (int i = 1; i < argc; i++) {
        if (!std::strncmp(argv[i], "-h", 2) || !std::strncmp(argv[i], "--help", 6)) {
            print_help();
            return EXIT_FAILURE;
        }

        if (!std::strncmp(argv[i], "-j", 2)) {
            errno = 0;

            thread_count = std::strtoull(argv[i] + 2, &endptr, 10);
            if (errno) {
                std::cerr << std::strerror(errno);
                return EXIT_FAILURE;
            }

            if (endptr == (argv[i] + 2) && i == argc - 1) {
                std::cerr << "invalid value passed to -j argument, quitting\n";
                return EXIT_FAILURE;
            } else if (endptr == (argv[i] + 2) && i < argc - 1) {
                errno = 0;

                thread_count = std::strtoull(argv[++i], &endptr, 10);
                if (errno) {
                    std::cerr << std::strerror(errno);
                    return EXIT_FAILURE;
                }

                if (*endptr != 0) {
                    std::cerr << "invalid value passed to -j argument, quitting\n";
                    return EXIT_FAILURE;
                }
            } else if (*endptr == 0)
                continue;

            continue;
        }

        if (!std::strncmp(argv[i], "-c", 2)) {
            errno = 0;

            cache_lifetime = std::strtoull(argv[i] + 2, &endptr, 10);
            if (errno) {
                std::cerr << std::strerror(errno);
                return EXIT_FAILURE;
            }

            if (endptr == (argv[i] + 2) && i == argc - 1) {
                std::cerr << "invalid value passed to -c argument, quitting\n";
                return EXIT_FAILURE;
            } else if (endptr == (argv[i] + 2) && i < argc - 1) {
                errno = 0;

                cache_lifetime = std::strtoull(argv[++i], &endptr, 10);
                if (errno) {
                    std::cerr << std::strerror(errno);
                    return EXIT_FAILURE;
                }

                if (*endptr != 0) {
                    std::cerr << "invalid value passed to -c argument, quitting\n";
                    return EXIT_FAILURE;
                }
            } else if (*endptr == 0)
                continue;

            continue;
        }

        if (!std::strncmp(argv[i], "-s", 2)) {
            errno = 0;

            entry_split = std::strtoull(argv[i] + 2, &endptr, 10);
            if (errno) {
                std::cerr << std::strerror(errno);
                return EXIT_FAILURE;
            }

            if (endptr == (argv[i] + 2) && i == argc - 1) {
                std::cerr << "invalid value passed to -s argument, quitting\n";
                return EXIT_FAILURE;
            } else if (endptr == (argv[i] + 2) && i < argc - 1) {
                errno = 0;

                entry_split = std::strtoull(argv[++i], &endptr, 10);
                if (errno) {
                    std::cerr << std::strerror(errno);
                    return EXIT_FAILURE;
                }

                if (*endptr != 0) {
                    std::cerr << "invalid value passed to -s argument, quitting\n";
                    return EXIT_FAILURE;
                }
            } else if (*endptr == 0)
                continue;

            continue;
        }

        if (!std::strncmp(argv[i], "-t", 2)) {
            print_stats = true;

            continue;
        }

        if (!input_path) {
            input_path = argv[i];
            continue;
        }

        if (input_path && !output_path) {
            output_path = argv[i];
            continue;
        }
    }

    thread_count = thread_count == 0 ? std::thread::hardware_concurrency() - 1 : thread_count;

    if (!input_path)
        input_path = ".nfsdb";

    if (!output_path)
        output_path = ".nfsdb.json";

    if (thread_count > 1)
        parser = std::make_unique<MultithreadedParser>(thread_count);
    else
        parser = std::make_unique<SinglethreadedParser>();

    if (cache_lifetime)
        parser->set_cache_lifetime(cache_lifetime);

    input.open(input_path);
    if (!input.good()) {
        std::cerr << "couldn't open " << input_path << " for reading, quitting\n";
        return EXIT_FAILURE;
    }

    file_size = std::filesystem::file_size(input_path, err);
    if (err) {
        no_progress = true;
        std::cout << "couldn't get " << input_path << " size, continuing with no progress bar\n";
    }

    std::cout << "Parsing events...\n";
    std::cout.flush();

    while (input.getline(line, 2048)) {
        if (!input.good()) {
            std::cerr << "error while reading the file, quitting\n";
            return EXIT_FAILURE;
        }

        /* Ignore first line (INITCWD) */
        if (!line_count++)
            continue;

        auto read = input.gcount();

        if (!no_progress && !(line_count % 10000) && isatty(STDOUT_FILENO)) {
            std::cout << total_read * 100 / file_size << "%\r";
            std::cout.flush();
        }

        auto ret = parser->parse_line(line, read, line_count);
        if (ret.is_error())
            std::cout << ret.explain();

        if (interrupt)
            break;

        total_read += read;
    }

    if (!interrupt) {
        std::cout << "100%\r";
    } else {
        std::cout << '\n';
        return 2;
    }

    parser->finish_parsing();

    input.close();

    auto results = parser->release_results();
    auto stats = parser->stats();

    if (print_stats) {
        std::cout << "Parsing statistics: \n";
        std::cout << "\tProcesses: " << stats.process_count << '\n';
        std::cout << "\tTotal event count: " << stats.total_event_count << '\n';
        std::cout << "\texec: " << stats.exec_count << '\n';
        std::cout << "\tfork: " << stats.fork_count << '\n';
        std::cout << "\tclose: " << stats.close_count << '\n';
        std::cout << "\topen: " << stats.open_count << '\n';
        std::cout << "\tpipe: " << stats.pipe_count << '\n';
        std::cout << "\tdup: " << stats.dup_count << '\n';
        std::cout << "\trename: " << stats.rename_count << '\n';
        std::cout << "\tlink: " << stats.link_count << '\n';
        std::cout << "\tsymlink: " << stats.symlink_count << '\n';
        std::cout << "\texit: " << stats.exit_count << '\n';
        std::cout << "\tMultilines: " << stats.multilines_count << '\n';
        std::cout << "\tProcesses at exit: " << stats.procs_at_exit << '\n';
        std::cout << "\tempty child count: " << stats.empty_child_count << '\n';
    }

    if (interrupt)
        return 2;

    /* Now create the pipe map
     *  Generally processes can communicate with each other using pipes. What do we want to achieve
     *  (for the purpose of dependency computation) is to know whether any child process could
     *  pass any data to the parent process.
     *  When a process executes pipe it creates two file descriptors (Nr,Mw).
     *  When it forks the child process inherits both descriptors (Nr,Mw).
     *  Now communication is possible when both file descriptors (for read and write) are open in the process
     *  at the time of cloning.
     *  The thing is the child will eventually call execve syscall and the process address space will be reset (and some
     *  file descriptors could be closed as well depending on the CLOEXEC flag) so we should probably focus on
     * possibility of communication between processes after the execve (which should reduce the false positive rate,
     * i.e. there is communication possibility considered during dependency computation whereas no information was
     * really possible to send). The pipe map will map any specific process into its parent for which there was an open
     * pipe for writing from this process to the parent at some point in time. Now the problem is that the root process
     * can create a pipe and spawn several children (which can spawn another children and so on). In this scenario each
     * process can talk with any other process using the same file descriptors. We handle that by assigning a unique
     * value to each file descriptor that originated from pipe to find opened ends of a given pipe in different
     * processes. Let's see an example from real life. `gcc` process calls a pipe creating two file descriptors =>
     * (3r,4w) and then forks and execve `cc1`. `cc1` duplicates 4w => 1w and closes (3r,4w). We're left with 1w in
     * `cc1`. `gcc` forks again and execve `as` (we still have (3r,4w). `as` duplicates 3r => 0r and closes (3r,4w).
     * We're left with 0r in `as`. This way we can write to stdout in `cc1` and read that back in `as` from stdin. Ok,
     * so how we'll do it? We will track file descriptors created by PIPE in all processes. Whenever we got the EXECVE
     * syscall we compare the file descriptor tables of the parent and the current process after EXECVE. If it's
     * possible to write to the parent using the same 'pipe_mark' file descriptors in both processes (write end in the
     * child and read end in the parent) we will add it to the 'pipe_mark'. Theoretically we should walk the process
     * tree and verify all the parents along the way but hey who sufficiently sane sends data through pipe to its great
     * grandparents? In entire Linux kernel build for Pixel 5 we add around 10% new entries to the pipe_map allowing
     * this. The same needs to be done for siblings. There's a problem though. There might be processes which spawn
     * thousands of children (root make or ninja process for example). Whenever new child is born we would need to
     * compare its file descriptor table with all the siblings. We can do this only for existing siblings at the time of
     * new exec to somehow alleviate this issue. Even with that there's still large number of superfluous connections
     * between siblings (which can ultimately lead to cycles). To further simplify this issue consider only connections
     * that come from stdout (1) to stdin (0) between siblings.
     */

    std::cout << "Creating pipe map...\n";
    std::cout.flush();

    pipe_map_t pipe_map;
    std::map<upid_t, unsigned> exeIdxMap;
    std::map<upid_t, std::set<upid_t>> fork_map;
    std::map<upid_t, upid_t> rev_fork_map;
    std::map<upid_t, fdmap_node*> fdmap_process_map;
    syscall_raw *root_sys = nullptr;
    fdmap_node *root_fdmap_node = nullptr;

    std::sort(results.syscalls.begin(), results.syscalls.end(),
            [](const syscall_raw &a, const syscall_raw &b) {
                return a.start_time < b.start_time;
            });

    /* Create root file descriptor map */
    if (results.syscalls.size() > 0) {
        root_sys = results.syscalls.data();
        root_fdmap_node = new fdmap_node(root_sys->pid);
        fdmap_process_map.insert(std::pair<upid_t,fdmap_node*>(root_sys->pid,root_fdmap_node));
    }

    /* Create dummy 'init' process */
    fork_map.insert(std::pair<upid_t, std::set<upid_t>>(0, std::set<upid_t>()));
#ifdef ENABLE_PARENT_PIPE_CHECK
    unsigned long parent_pipe_count = 0;
#endif

    uint32_t pipe_index = 0;
    size_t progress_counter = 0;
    size_t syscalls_size = results.syscalls.size();
    for (auto i = results.syscalls.begin(); i != results.syscalls.end(); ++i) {
        progress_counter++;
        if (isatty(STDOUT_FILENO) && !(progress_counter % 100)) {
            std::cout << (progress_counter * 100) / syscalls_size << "%\r";
            std::cout.flush();
        }

        syscall_raw &sys = (*i);
        if (sys.sysname == syscall_raw::SYS_PIPE) {
            /*
             * i0 -> fd1
             * i1 -> fd2
             * ul -> flags
             */
            if (fdmap_process_map.find(sys.pid) == fdmap_process_map.end()) {
                std::cerr << "WARNING: no file descriptor table for process (" << sys.pid << ") at PIPE [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
                continue;
            }
            fdmap_node *fdmap = fdmap_process_map[sys.pid];
            fdmap->fdmap.insert(std::pair<int, fdinfo>(sys.i0, fdinfo(1, 0, (sys.ul & O_CLOEXEC) != 0, pipe_index)));
            fdmap->fdmap.insert(std::pair<int, fdinfo>(sys.i1, fdinfo(0, 1, (sys.ul & O_CLOEXEC) != 0, pipe_index)));
            pipe_index++;
        } else if (sys.sysname == syscall_raw::SYS_DUP) {
            /*
             * i0 -> oldfd
             * i1 -> newfd
             * ul -> flags
             */
            if (fdmap_process_map.find(sys.pid) == fdmap_process_map.end()) {
                std::cerr << "WARNING: no file descriptor table for process (" << sys.pid << ") at PIPE [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
                continue;
            }
            fdmap_node *fdmap = fdmap_process_map[sys.pid];
            if (fdmap->fdmap.find(sys.i0) == fdmap->fdmap.end()) {
                /* We can duplicate fd which didn't originate from PIPE (and therefore isn't tracked) */
                continue;
            }
            struct fdinfo &old_fdinfo = fdmap->fdmap[sys.i0];
            if (fdmap->fdmap.find(sys.i1) != fdmap->fdmap.end()) {
                /* Silently close the destination fd */
                fdmap->fdmap.erase(sys.i1);
            }
            fdmap->fdmap.insert(std::pair<int, fdinfo>(
                sys.i1, fdinfo(old_fdinfo.piperd, old_fdinfo.pipewr, (sys.ul & O_CLOEXEC) != 0, old_fdinfo.pipe_mark)));
        } else if (sys.sysname == syscall_raw::SYS_FORK) {
            /*
             * sv -> pid of fork?
             * ul -> flags
             */
            exeIdxMap.insert(std::pair<upid_t, unsigned>(sys.pv, 0));
            if (fdmap_process_map.find(sys.pid) == fdmap_process_map.end()) {
                std::cerr << "WARNING: no file descriptor table for process (" << sys.pid << ") at PIPE [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
                continue;
            }
            fdmap_node *fdmap = fdmap_process_map[sys.pid];
            if (fdmap_process_map.find(sys.pv) != fdmap_process_map.end()) {
                std::cerr << "WARNING: file descriptor map for new process (" << sys.pv << ") spawned at " << sys.pid << " at FORK already in map [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
                continue;
            }
            if (sys.ul & CLONE_FILES) {
                /* Share the file descriptor table */
                fdmap_process_map.insert(std::pair<upid_t, fdmap_node *>(sys.pv, fdmap));
                fdmap->refs.insert(sys.pv);
            } else {
                /* Copy the file descriptor table */
                fdmap_node *new_fdmap_node = new fdmap_node(sys.pv);
                fdmap_process_map.insert(std::pair<upid_t, fdmap_node *>(sys.pv, new_fdmap_node));
                for (auto u = fdmap->fdmap.begin(); u != fdmap->fdmap.end(); ++u) {
                    new_fdmap_node->fdmap.insert(
                        std::pair<int, fdinfo>((*u).first, fdinfo((*u).second.piperd, (*u).second.pipewr,
                                                                  (*u).second.cloexec, (*u).second.pipe_mark)));
                }
            }
            if (rev_fork_map.find(sys.pv) != rev_fork_map.end()) {
                std::cerr << "WARNING: parent process entry for process (" << sys.pv << ") spawned at (" << sys.pid << ") at FORK already in reverse process map [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
                continue;
            }
            rev_fork_map.insert(std::pair<upid_t, upid_t>(sys.pv, sys.pid));
            if (fork_map.find(sys.pid) == fork_map.end()) {
                fork_map.insert(std::pair<upid_t, std::set<upid_t>>(sys.pid, std::set<upid_t>()));
            }
            fork_map[sys.pid].insert(sys.pv);
        } else if (sys.sysname == syscall_raw::SYS_EXEC) {
            if (exeIdxMap.find(sys.pid) == exeIdxMap.end()) {
                exeIdxMap.insert(std::pair<upid_t, unsigned>(sys.pid, 1));
            } else {
                exeIdxMap[sys.pid]++;
            }
            if (fdmap_process_map.find(sys.pid) == fdmap_process_map.end()) {
                std::cerr << "WARNING: no file descriptor table for process (" << sys.pid << ") at EXECVE [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
                continue;
            }
            fdmap_node *fdmap = fdmap_process_map[sys.pid];
            /* Unshare file descriptor map (if it was shared between processes) */
            if (fdmap->refs.size() > 1) {
                fdmap_node *new_fdmap_node = new fdmap_node(sys.pid);
                for (auto u = fdmap->fdmap.begin(); u != fdmap->fdmap.end(); ++u) {
                    new_fdmap_node->fdmap.insert(
                        std::pair<int, fdinfo>((*u).first, fdinfo((*u).second.piperd, (*u).second.pipewr,
                                                                  (*u).second.cloexec, (*u).second.pipe_mark)));
                }
                if (fdmap->refs.find(sys.pid) == fdmap->refs.end()) {
                    std::cerr << "ERROR: missing identifier for shared file descriptor table for process (" << sys.pid;
                    std::cerr << ") at EXECVE [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
                    return 1;
                }
                fdmap->refs.erase(sys.pid);
                fdmap_process_map[sys.pid] = new_fdmap_node;
            }
            fdmap = fdmap_process_map[sys.pid];
            /* Close file descriptors marked as CLOEXEC */
            for (auto u = fdmap->fdmap.begin(); u != fdmap->fdmap.end(); ) {
                auto &descriptor = u->second;
                if (descriptor.cloexec) {
                    u = fdmap->fdmap.erase(u);
                    continue;
                }
                ++u;
            }
            if (rev_fork_map.find(sys.pid) == rev_fork_map.end()) {
                std::cerr << "WARNING: could not find active parent for process (" << sys.pid;
                std::cerr << ") at EXECVE [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
                continue;
            }
            upid_t ppid = rev_fork_map[sys.pid];
            if (fdmap_process_map.find(ppid) == fdmap_process_map.end()) {
                std::cerr << "WARNING: no file descriptor table for process (" << ppid << ") at EXECVE";
                std::cerr << " [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
                continue;
            }
            fdmap_node *parent_fdmap = fdmap_process_map[ppid];
            /* Look for write fd in the current process and read fd in the parent with the same 'pipe_mark' */
            if (pipe_open_for_write(fdmap, parent_fdmap)) {
                update_pipe_map(pipe_map, sys.pid, exeIdxMap[sys.pid], ppid, exeIdxMap[ppid]);
            }
#ifdef ENABLE_PARENT_PIPE_CHECK
            /* Check the parents? */
            while (rev_fork_map.find(ppid) != rev_fork_map.end()) {
                upid_t nppid = rev_fork_map[ppid];
                if (fdmap_process_map.find(nppid) != fdmap_process_map.end()) {
                    fdmap_node *nparent_fdmap = fdmap_process_map[nppid];
                    if (pipe_open_for_write(fdmap, nparent_fdmap)) {
                        parent_pipe_count++;
                        update_pipe_map(pipe_map, sys.pid, nppid);
                    }
                }
                ppid = nppid;
            }
#endif
#ifdef ENABLE_SIBLING_PIPE_CHECK
            /* Now the siblings */
            if (fork_map.find(ppid) == fork_map.end()) {
                std::cerr << "WARNING: no entry in process map for parent process (" << ppid;
                std::cerr << ") at EXECVE [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
                continue;
            }
            std::set<upid_t> &siblings_with_me = fork_map[ppid];
            for (auto u = siblings_with_me.begin(); u != siblings_with_me.end(); ++u) {
                if (*u != sys.pid) {
                    if (fdmap_process_map.find(*u) != fdmap_process_map.end()) {
                        fdmap_node *sibling_fdmap = fdmap_process_map[*u];
                        if (pipe_open_for_write(fdmap, sibling_fdmap, 1)) {
                            update_pipe_map(pipe_map, sys.pid, exeIdxMap[sys.pid], *u, exeIdxMap[*u]);
                        }
                        if (pipe_open_for_write(sibling_fdmap, fdmap, 1)) {
                            update_pipe_map(pipe_map, *u, exeIdxMap[*u], sys.pid, exeIdxMap[sys.pid]);
                        }
                    }
                }
            }
#endif

        } else if (sys.sysname == syscall_raw::SYS_CLOSE) {
            /*
             * i0 -> fd
             * i1 -> -1
             * ul -> 0
             */
            if (fdmap_process_map.find(sys.pid) == fdmap_process_map.end()) {
                std::cerr << "WARNING: no file descriptor table for process (" << sys.pid;
                std::cerr << ") at CLOSE [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
                continue;
            }
            fdmap_node *fdmap = fdmap_process_map[sys.pid];
            if (fdmap->fdmap.find(sys.i0) != fdmap->fdmap.end()) {
                fdmap->fdmap.erase(sys.i0);
            } else {
                /* We can close fd which didn't originate from PIPE (and therefore isn't tracked) */
            }
        } else if (sys.sysname == syscall_raw::SYS_EXIT) {
            if (rev_fork_map.find(sys.pid) == rev_fork_map.end()) {
                std::cerr << "WARNING: no entry in reverse process map for process (" << sys.pid;
                std::cerr << ") at EXIT [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
                continue;
            }
            /* Remove this process from its parent children */
            upid_t ppid = rev_fork_map[sys.pid];
            if (fork_map.find(ppid) == fork_map.end()) {
                std::cerr << "WARNING: no entry in process map for parent process (" << ppid;
                std::cerr << ") at EXIT [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
            } else {
                std::set<upid_t> &pchlds = fork_map[ppid];
                pchlds.erase(sys.pid);
            }
            rev_fork_map.erase(sys.pid);
            if (fork_map.find(sys.pid) == fork_map.end()) {
                /* No children */
                continue;
            }
            std::set<upid_t> &chlds = fork_map[sys.pid];
            if (chlds.size() > 0) {
                /* Process exits but some children still active; pass it to 'init' process */
                for (auto u = chlds.begin(); u != chlds.end(); ++u) {
                    rev_fork_map[*u] = 0;
                }
                std::set<upid_t> &root_chlds = fork_map[0];
                root_chlds.insert(chlds.begin(), chlds.end());
            }
            if (fdmap_process_map.find(sys.pid) == fdmap_process_map.end()) {
                std::cerr << "WARNING: no file descriptor table for process (" << sys.pid;
                std::cerr << ") at EXIT [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
                continue;
            }
            fdmap_node *fdmap = fdmap_process_map[sys.pid];
            if (fdmap->refs.size() > 1) {
                if (fdmap->refs.find(sys.pid) == fdmap->refs.end()) {
                    std::cerr << "ERROR: missing identifier for shared file descriptor table for process (" << sys.pid;
                    std::cerr << ") at EXIT [" << std::distance(results.syscalls.begin(), i) << "|" << sys.start_time << "]\n";
                    return 1;
                }
                fdmap->refs.erase(sys.pid);
            } else {
                delete fdmap;
            }
            fdmap_process_map.erase(sys.pid);
            fork_map.erase(sys.pid);
        }
        if (interrupt)
            break;
    } /* for(i) */
    if (!interrupt)
        std::cout << "100%\r";
    else
        std::cout << '\n';
    if (interrupt)
        return 2;
    for (auto u = fdmap_process_map.begin(); u != fdmap_process_map.end(); ++u) {
        if ((*u).second->refs.size() > 1) {
            if ((*u).second->refs.find((*u).first) == (*u).second->refs.end()) {
                std::cerr << "ERROR: missing identifier for shared file descriptor table for process (" << (*u).first;
                std::cerr << ") at destructor\n";
                return 1;
            }
            (*u).second->refs.erase((*u).first);
        } else {
            delete (*u).second;
        }
    }
    unsigned long pipe_map_entries = 0;
    for (auto u = pipe_map.begin(); u != pipe_map.end(); ++u)
        pipe_map_entries += (*u).second.size();

    std::cout << "pipe_map size at exit: " << pipe_map.size() << " keys, " << pipe_map_entries << " entries total\n";
#ifdef ENABLE_PARENT_PIPE_CHECK
    std::cout << "parent pipe count: " << parent_pipe_count << '\n';
#endif

    output.open(output_path);
    if (!output.good()) {
        std::cerr << "couldn't open " << output_path << " for writing, quitting\n";
        return EXIT_FAILURE;
    }

    flush_entries(results, pipe_map, output, entry_split);

    if (!interrupt)
        std::cout << "\r100%\r";
    else
        std::cout << "\n";

    if (interrupt)
        return 2;

    output.close();

    return 0;
}

int main(int argc, char **argv) {
    return parser_main(argc, argv);
}
