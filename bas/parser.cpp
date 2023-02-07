#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <fcntl.h>
#include "utils.h"
#include "parser.h"
/* Generated using 'gperf phash.txt > phash.h' */
#include "phash.h"
#include <map>

static unsigned int simple_hash(const char *s) {
    return hash(s, strlen(s));
}

static unsigned int simple_hashn(const char *s, size_t len) {
    return hash(s, len);
}

static int process_arg(char *arg, enum SYSEVENT cmd, void *cmdArgs) {

    char *eargname = strchr(arg, '=');
    *eargname = 0;

    switch (simple_hash(arg)) {
    case PHASH_STR(pid): {
        assert(cmd == SYSEVENT_SCHEDFORK);
        if (sscanf(eargname + 1, SYSEVENT_SCHEDFORK_ARG_PID_FMT, &((struct SchedForkArgs *)cmdArgs)->cpid) == 1) {
            goto done;
        }
    } break;
    case PHASH_STR(argsize): {
        assert(cmd == SYSEVENT_NEWPROC);
        if (sscanf(eargname + 1, SYSEVENT_NEWPROC_ARG_ARGSIZE_FMT, &((struct NewProcArgs *)cmdArgs)->argsize) == 1) {
            goto done;
        }
    } break;
    case PHASH_STR(prognameisize): {
        assert(cmd == SYSEVENT_NEWPROC);
        if (sscanf(eargname + 1, SYSEVENT_NEWPROC_ARG_PROGNAMEISIZE_FMT,
                   &((struct NewProcArgs *)cmdArgs)->prognameisize) == 1) {
            goto done;
        }
    } break;
    case PHASH_STR(prognamepsize): {
        assert(cmd == SYSEVENT_NEWPROC);
        if (sscanf(eargname + 1, SYSEVENT_NEWPROC_ARG_PROGNAMEPSIZE_FMT,
                   &((struct NewProcArgs *)cmdArgs)->prognamepsize) == 1) {
            goto done;
        }
    } break;
    case PHASH_STR(cwdsize): {
        assert(cmd == SYSEVENT_NEWPROC);
        if (sscanf(eargname + 1, SYSEVENT_NEWPROC_ARG_CWDSIZE_FMT, &((struct NewProcArgs *)cmdArgs)->cwdsize) == 1) {
            goto done;
        }
    } break;
    case PHASH_STR(fd): {
        assert((cmd == SYSEVENT_CLOSE) || (cmd == SYSEVENT_OPEN));
        if (cmd == SYSEVENT_CLOSE) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FD_FMT, &((struct CloseArgs *)cmdArgs)->fd) == 1) {
                goto done;
            }
        } else if (cmd == SYSEVENT_OPEN) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FD_FMT, &((struct OpenArgs *)cmdArgs)->fd) == 1) {
                goto done;
            }
        }
    } break;
    case PHASH_STR(fd1): {
        assert(cmd == SYSEVENT_PIPE);
        if (sscanf(eargname + 1, SYSEVENT__ARG_FD_FMT, &((struct PipeArgs *)cmdArgs)->fd1) == 1) {
            goto done;
        }
    } break;
    case PHASH_STR(fd2): {
        assert(cmd == SYSEVENT_PIPE);
        if (sscanf(eargname + 1, SYSEVENT__ARG_FD_FMT, &((struct PipeArgs *)cmdArgs)->fd2) == 1) {
            goto done;
        }
    } break;
    case PHASH_STR(oldfd): {
        assert(cmd == SYSEVENT_DUP);
        if (sscanf(eargname + 1, SYSEVENT__ARG_FD_FMT, &((struct DupArgs *)cmdArgs)->oldfd) == 1) {
            goto done;
        }
    } break;
    case PHASH_STR(newfd): {
        assert(cmd == SYSEVENT_DUP);
        if (sscanf(eargname + 1, SYSEVENT__ARG_FD_FMT, &((struct DupArgs *)cmdArgs)->newfd) == 1) {
            goto done;
        }
    } break;
    case PHASH_STR(flags): {
        assert((cmd == SYSEVENT_SYSCLONE) || (cmd == SYSEVENT_OPEN) || (cmd == SYSEVENT_RENAME2FROM) ||
               (cmd == SYSEVENT_LINKATFROM) || (cmd == SYSEVENT_PIPE) || (cmd == SYSEVENT_DUP));
        if (cmd == SYSEVENT_SYSCLONE) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FLAGS_FMT, &((struct SysCloneArgs *)cmdArgs)->flags) == 1) {
                goto done;
            }
        } else if (cmd == SYSEVENT_OPEN) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FLAGS_FMT, &((struct OpenArgs *)cmdArgs)->flags) == 1) {
                goto done;
            }
        } else if (cmd == SYSEVENT_RENAME2FROM) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FLAGS_FMT, &((struct RenameArgs *)cmdArgs)->flags) == 1) {
                goto done;
            }
        } else if (cmd == SYSEVENT_LINKATFROM) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FLAGS_FMT, &((struct LinkArgs *)cmdArgs)->flags) == 1) {
                goto done;
            }
        } else if (cmd == SYSEVENT_PIPE) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FLAGS_FMT, &((struct PipeArgs *)cmdArgs)->flags) == 1) {
                goto done;
            }
        } else if (cmd == SYSEVENT_DUP) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FLAGS_FMT, &((struct DupArgs *)cmdArgs)->flags) == 1) {
                goto done;
            }
        }
    } break;
    case PHASH_STR(mode): {
        assert(cmd == SYSEVENT_OPEN);
        if (sscanf(eargname + 1, SYSEVENT_OPEN_ARG_MODE_FMT, &((struct OpenArgs *)cmdArgs)->mode) == 1) {
            goto done;
        }
    } break;
    case PHASH_STR(fnamesize): {
        assert((cmd == SYSEVENT_OPEN) || (cmd == SYSEVENT_RENAMEFROM) || (cmd == SYSEVENT_RENAME2FROM) ||
               (cmd == SYSEVENT_RENAMETO) || (cmd == SYSEVENT_LINKFROM) || (cmd == SYSEVENT_LINKATFROM) ||
               (cmd == SYSEVENT_LINKTO));
        if (cmd == SYSEVENT_OPEN) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FNAMESIZE_FMT, &((struct OpenArgs *)cmdArgs)->fnamesize) == 1) {
                goto done;
            }
        } else if (cmd == SYSEVENT_RENAMEFROM) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FNAMESIZE_FMT, &((struct RenameArgs *)cmdArgs)->fnamesize) == 1) {
                goto done;
            }
        } else if (cmd == SYSEVENT_RENAME2FROM) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FNAMESIZE_FMT, &((struct RenameArgs *)cmdArgs)->fnamesize) == 1) {
                goto done;
            }
        } else if (cmd == SYSEVENT_RENAMETO) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FNAMESIZE_FMT, &((struct RenameArgs *)cmdArgs)->fnamesize) == 1) {
                goto done;
            }
        } else if (cmd == SYSEVENT_LINKFROM) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FNAMESIZE_FMT, &((struct LinkArgs *)cmdArgs)->fnamesize) == 1) {
                goto done;
            }
        } else if (cmd == SYSEVENT_LINKATFROM) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FNAMESIZE_FMT, &((struct LinkArgs *)cmdArgs)->fnamesize) == 1) {
                goto done;
            }
        } else if (cmd == SYSEVENT_LINKTO) {
            if (sscanf(eargname + 1, SYSEVENT__ARG_FNAMESIZE_FMT, &((struct LinkArgs *)cmdArgs)->fnamesize) == 1) {
                goto done;
            }
        }
    } break;
    case PHASH_STR(forigsize): {
        assert(cmd == SYSEVENT_OPEN);
        if (sscanf(eargname + 1, SYSEVENT__ARG_FNAMESIZE_FMT, &((struct OpenArgs *)cmdArgs)->forigsize) == 1) {
            goto done;
        }
    } break;
    case PHASH_STR(targetnamesize): {
        assert(cmd == SYSEVENT_SYMLINK);
        if (sscanf(eargname + 1, SYSEVENT_SYMLINK_ARG_TARGETNAMESIZE_FMT,
                   &((struct SymLinkArgs *)cmdArgs)->targetnamesize) == 1) {
            goto done;
        }
    } break;
    case PHASH_STR(linknamesize): {
        assert(cmd == SYSEVENT_SYMLINK);
        if (sscanf(eargname + 1, SYSEVENT_SYMLINK_ARG_LINKNAMESIZE_FMT,
                   &((struct SymLinkArgs *)cmdArgs)->linknamesize) == 1) {
            goto done;
        }
    } break;
    case PHASH_STR(resolvednamesize): {
        assert(cmd == SYSEVENT_SYMLINK);
        if (sscanf(eargname + 1, SYSEVENT_SYMLINK_ARG_RESOLVEDNAMESIZE_FMT,
                   &((struct SymLinkArgs *)cmdArgs)->resolvednamesize) == 1) {
            goto done;
        }
    } break;
    }

    return 0;

done:
    *eargname = '=';
    return 1;
}

int parse_generic_args(const char *line, eventTuple_t *evln) {
    char *event_separator;
    char *argument_bracket;
    char *endptr;
    unsigned long long pid;
    unsigned long cpu;
    unsigned long time;
    unsigned long timen;

    errno = 0;
    pid = strtoull(line, &endptr, 10);
    if (errno || *endptr != ',') /* [[unlikely]] */
        return 0;
    line = ++endptr;
    cpu = strtoul(line, &endptr, 10);
    if (errno || *endptr != ',') /* [[unlikely]] */
        return 0;
    line = ++endptr;
    time = strtoul(line, &endptr, 10);
    if (errno || *endptr != ',') /* [[unlikely]] */
        return 0;
    line = ++endptr;
    timen = strtoul(line, &endptr, 10);
    if (errno || *endptr != '!') /* [[unlikely]] */
        return 0;
    line = ++endptr;

    event_separator = const_cast<char *>(strchr(line, '|'));

    if (!event_separator) {
        argument_bracket = const_cast<char *>(line);
        if ((*argument_bracket == 'A') && (*(++argument_bracket) == '[')) {
            while (isdigit(*(++argument_bracket)))
                ;
            if (*argument_bracket != ']')
                return 0;
        }
    }

    evln->pid = pid;
    evln->cpu = cpu;
    evln->timen = time * 1000000000UL + timen;
    evln->event = simple_hashn(line, event_separator - line);
    evln->event_line = strdup(line);
    return 1;
}

static int parse_args(char *args, enum SYSEVENT cmd, void *cmdArgs) {

    char *beg = args;
    char *end = 0;
    do {
        end = strchr(beg, ',');
        if (end) {
            *end = 0;
            int rv = process_arg(beg, cmd, cmdArgs);
            *end = ',';
            if (!rv)
                return 0;
            beg = end + 1;
        } else {
            if (!process_arg(beg, cmd, cmdArgs))
                return 0;
        }
    } while (end);
    return 1;
}

static char *get_longpath_arg(vpeventTuple_t *event_list, unsigned long *vi, const char *tag, const char *tag_end) {

    char *longfn = 0;
    long cargidx = -1;
    while (*vi < VEC_SIZE(*event_list)) {
        eventTuple_t *ievln = VEC_ACCESS(*event_list, *vi);
        if ((ievln->event_line[0] == tag[0]) && (ievln->event_line[1] == tag[1]) && (ievln->event_line[2] == '|')) {
            return strndup(ievln->event_line + 3, strlen(ievln->event_line + 3) - 1);
        } else if ((ievln->event_line[0] == tag[0]) && (ievln->event_line[1] == tag[1]) &&
                   (ievln->event_line[2] == '[')) {
            char *cmde = strchr(ievln->event_line + 3, ']');
            *cmde = 0;
            long argidx;
            if (sscanf(ievln->event_line + 3, "%ld", &argidx) != 1) {
                return 0;
            }
            *cmde = ']';
            // Beware on trailing newlines
            size_t slen = strlen(cmde + 1) - 1;
            if (argidx == 0) {
                longfn = strndup(cmde + 1, slen);
            } else {
                if (argidx != cargidx + 1) {
                    return 0;
                }
                longfn = strnappend(longfn, cmde + 1, slen);
            }
            cargidx = argidx;
        } else if (strcmp(ievln->event_line, tag_end) == 0) {
            return longfn;
        } else {
            return 0;
        }
        (*vi)++;
    }
    return 0;
}

long parse_write_process_events(upid_t pid, vpeventTuple_t *event_list, struct parse_context *context,
                                uint64_t start_time, uint64_t end_time) {

    unsigned long vi = 0;
    long n_processed = 0;
    long n_execs = 0;
    unsigned procidx = 0;
    uint64_t exe_start_time = start_time;
    size_t ve_init_size = context->ve.size();
    context->ve.push_back(parsed_entry(pid));
    context->pset.insert(pid);
    std::vector<syscall_raw> &srvec = context->srvec;
    size_t srvec_init_size = srvec.size();

    while (vi < VEC_SIZE(*event_list)) {
        eventTuple_t *evln = VEC_ACCESS(*event_list, vi);
        char *cmde = strchr(evln->event_line, '|');
        uint8_t event = simple_hashn(evln->event_line, cmde - evln->event_line);
        switch (event) {
        case PHASH_STR(New_proc): {
            struct NewProcArgs argnfo = {};
            if (!parse_args(cmde + 1, SYSEVENT_NEWPROC, &argnfo)) {
                fprintf(stderr, "ERROR: Failed to process args at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi, evln->pid,
                        evln->event_line);
                goto error;
            }
            /* Read exec arguments */
            int done = 0;
            long cargidx = -1;
            char *longargs = 0;
            VEC_INIT(argnfo.argv);
            ++vi;
            while (vi < VEC_SIZE(*event_list)) {
                eventTuple_t *ievln = VEC_ACCESS(*event_list, vi);
                if ((ievln->event_line[0] == 'P') && (ievln->event_line[1] == 'I')) {
                    char *longfn = get_longpath_arg(event_list, &vi, "PI", "PI_end\n");
                    if (!longfn) {
                        fprintf(stderr, "ERROR: Invalid exec argument line [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                                ievln->pid, ievln->event_line);
                        goto error;
                    }
                    if (strlen(longfn) != argnfo.prognameisize) {
                        fprintf(
                            stderr,
                            "ERROR: Size mismatch in progname_i (%zu vs %zu) exec argument [%lu]: [" GENERIC_ARG_PID_FMT
                            "] %s\n",
                            strlen(longfn), argnfo.prognameisize, vi, ievln->pid, ievln->event_line);
                        goto error;
                    }
                    argnfo.progname_i = longfn;
                } else if ((ievln->event_line[0] == 'P') && (ievln->event_line[1] == 'P')) {
                    char *longfn = get_longpath_arg(event_list, &vi, "PP", "PP_end\n");
                    if (!longfn) {
                        fprintf(stderr, "ERROR: Invalid exec argument line [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                                ievln->pid, ievln->event_line);
                        goto error;
                    }
                    if (strlen(longfn) != argnfo.prognamepsize) {
                        fprintf(
                            stderr,
                            "ERROR: Size mismatch in progname_p (%zu vs %zu) exec argument [%lu]: [" GENERIC_ARG_PID_FMT
                            "] %s\n",
                            strlen(longfn), argnfo.prognamepsize, vi, ievln->pid, ievln->event_line);
                        goto error;
                    }
                    argnfo.progname_p = longfn;
                } else if ((ievln->event_line[0] == 'C') && (ievln->event_line[1] == 'W')) {
                    char *longfn = get_longpath_arg(event_list, &vi, "CW", "CW_end\n");
                    if (!longfn) {
                        fprintf(stderr, "ERROR: Invalid exec argument line [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                                ievln->pid, ievln->event_line);
                        goto error;
                    }
                    if (strlen(longfn) != argnfo.cwdsize) {
                        fprintf(stderr,
                                "ERROR: Size mismatch in cwd (%zu vs %zu) exec argument [%lu]: [" GENERIC_ARG_PID_FMT
                                "] %s\n",
                                strlen(longfn), argnfo.cwdsize, vi, ievln->pid, ievln->event_line);
                        goto error;
                    }
                    argnfo.cwd = longfn;
                } else if ((ievln->event_line[0] == 'A') && (ievln->event_line[1] == '[')) {
                    char *cmde = strchr(ievln->event_line + 2, ']');
                    *cmde = 0;
                    long argidx;
                    if (sscanf(ievln->event_line + 2, "%ld", &argidx) != 1) {
                        fprintf(stderr, "ERROR: Invalid exec argument line [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                                ievln->pid, ievln->event_line);
                        goto error;
                    }
                    *cmde = ']';
                    // Beware on trailing newlines
                    size_t slen = strlen(cmde + 1) - 1;
                    if (argidx == cargidx) {
                        // Keep another part of the string
                        longargs = strnappend(longargs, cmde + 1, slen);
                    } else {
                        if (argidx > 0) {
                            VEC_APPEND(const char *, argnfo.argv, longargs);
                        }
                        // Setup new string for processing
                        cargidx = argidx;
                        longargs = strndup(cmde + 1, slen);
                    }
                } else if (ievln->event_line[0] == 'E') {
                    char *cmde = strchr(ievln->event_line, '|');
                    if (simple_hashn(ievln->event_line, cmde - ievln->event_line) == PHASH_STR(End_of_args)) {
                        procidx++;
                        VEC_APPEND(const char *, argnfo.argv, longargs);
                        unsigned long ai;
                        uint64_t utn = (uint64_t)evln->timen;
                        context->ve.back().settime(utn - exe_start_time);
                        MAKE_JSON_ESCAPED(_progname_p, argnfo.progname_p);
                        MAKE_JSON_ESCAPED(_cwd, argnfo.cwd);
                        const char *p = path_join(_cwd, _progname_p);
                        struct parsed_entry e(evln->pid, procidx, p ? p : _progname_p, _cwd);
                        context->ve.push_back(e);
                        exe_start_time = utn;
                        if ((context->total_event_count + n_processed > 0) && (context->rawoutfd))
                            fprintf(context->rawoutfd, ",\n");
                        if (context->rawoutfd)
                            fprintf(context->rawoutfd,
                                    "{\"c\":\"e\",\"p\":" GENERIC_ARG_PID_FMT ",\"x\":%u,\"t\":%" PRIu64
                                    ",\"b\":\"%s\",\"w\":\"%s\",\"v\":[",
                                    evln->pid, procidx, utn - context->start_time, p ? p : _progname_p, _cwd);
                        free((void *)p);
                        FREE_JSON_ESCAPED(_progname_p);
                        FREE_JSON_ESCAPED(_cwd);
                        for (ai = 0; ai < VEC_SIZE(argnfo.argv); ++ai) {
                            MAKE_JSON_ESCAPED(argi, VEC_ACCESS(argnfo.argv, ai));
                            context->ve.back().argv.push_back(argi);
                            if (context->rawoutfd)
                                fprintf(context->rawoutfd, "\"%s\"", argi);
                            FREE_JSON_ESCAPED(argi);
                            if ((ai < VEC_SIZE(argnfo.argv) - 1) && (context->rawoutfd))
                                fprintf(context->rawoutfd, ",");
                        }
                        if (context->rawoutfd)
                            fprintf(context->rawoutfd, "]}");
                        srvec.push_back(syscall_raw(pid, syscall_raw::SYS_EXEC, utn - context->start_time));
                        free((void *)argnfo.progname_i);
                        free((void *)argnfo.progname_p);
                        free((void *)argnfo.cwd);
                        for (ai = 0; ai < VEC_SIZE(argnfo.argv); ++ai) {
                            free((void *)VEC_ACCESS(argnfo.argv, ai));
                        }
                        VEC_DESTROY(argnfo.argv);
                        done = 1;
                        n_processed++;
                        n_execs++;
                        context->event_count.exec++;
                        break;
                    } else {
                        fprintf(stderr, "ERROR: Invalid exec argument line [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                                ievln->pid, ievln->event_line);
                        goto error;
                    }
                } else {
                    fprintf(stderr, "ERROR: Invalid exec argument line [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                            ievln->pid, ievln->event_line);
                    goto error;
                }
                ++vi;
            }
            if (!done) {
                fprintf(stderr, "ERROR: Failed to process exec args [%lu]\n", vi);
                goto error;
            }
            break;
        }
        case PHASH_STR(SchedFork): {
            struct SchedForkArgs argnfo = {};
            if (!parse_args(cmde + 1, SYSEVENT_SCHEDFORK, &argnfo)) {
                fprintf(stderr, "ERROR: Failed to process args at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi, evln->pid,
                        evln->event_line);
                goto error;
            }
            uint64_t utn = (uint64_t)evln->timen;
            struct parsed_entry &e = context->ve.back();
            e.vchild.push_back(std::pair<upid_t, unsigned long>(argnfo.cpid, 0));
            if ((context->total_event_count + n_processed > 0) && (context->rawoutfd))
                fprintf(context->rawoutfd, ",\n");
            if (context->rawoutfd)
                fprintf(context->rawoutfd,
                        "{\"c\":\"f\",\"p\":" GENERIC_ARG_PID_FMT ",\"x\":%u,\"t\":%" PRIu64
                        ",\"h\":" GENERIC_ARG_PID_FMT ",\"f\":%lu}",
                        evln->pid, procidx, utn - context->start_time, argnfo.cpid, (unsigned long)0);
            assert(context->rev_fork_map.find(argnfo.cpid) == context->rev_fork_map.end());
            context->rev_fork_map.insert(std::pair<upid_t, std::pair<upid_t, unsigned>>(
                argnfo.cpid, std::pair<upid_t, unsigned>(evln->pid, context->ve.size() - ve_init_size - 1)));
            n_processed++;
            context->event_count.fork++;
            srvec.push_back(syscall_raw(pid, syscall_raw::SYS_FORK, argnfo.cpid, 0, utn - context->start_time));
        } break;
        case PHASH_STR(SysClone): {
            struct SysCloneArgs argnfo = {};
            if (!parse_args(cmde + 1, SYSEVENT_SYSCLONE, &argnfo)) {
                fprintf(stderr, "ERROR: Failed to process args at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi, evln->pid,
                        evln->event_line);
                goto error;
            }
            /* There should be SchedFork or SysCloneFailed event following Clone */
            ++vi;
            if (vi >= VEC_SIZE(*event_list)) {
                fprintf(stderr,
                        "WARNING: Missing SchedFork/SysCloneFailed after Clone at [%lu]: [" GENERIC_ARG_PID_FMT
                        "] %s\n",
                        vi, evln->pid, evln->event_line);
                break;
            }
            eventTuple_t *ievln = VEC_ACCESS(*event_list, vi);
            char *cmde = strchr(ievln->event_line, '|');
            uint8_t event = simple_hashn(ievln->event_line, cmde - ievln->event_line);
            if ((event != PHASH_STR(SchedFork)) && (event != PHASH_STR(SysCloneFailed))) {
                fprintf(stderr,
                        "WARNING: Missing SchedFork/SysCloneFailed after Clone at [%lu]: [" GENERIC_ARG_PID_FMT
                        "] %s\n",
                        vi, ievln->pid, ievln->event_line);
                vi--;
                break;
            }
            if (event == PHASH_STR(SysCloneFailed)) {
                /* Nothing to do, just keep going */
            } else {
                struct SchedForkArgs argnfo_fork = {};
                if (!parse_args(cmde + 1, SYSEVENT_SCHEDFORK, &argnfo_fork)) {
                    fprintf(stderr, "ERROR: Failed to process args at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                            ievln->pid, ievln->event_line);
                    goto error;
                }
                uint64_t utn = (uint64_t)evln->timen;
                struct parsed_entry &e = context->ve.back();
                e.vchild.push_back(std::pair<upid_t, unsigned long>(argnfo_fork.cpid, argnfo.flags));
                if ((context->total_event_count + n_processed > 0) && (context->rawoutfd))
                    fprintf(context->rawoutfd, ",\n");
                if (context->rawoutfd)
                    fprintf(context->rawoutfd,
                            "{\"c\":\"f\",\"p\":" GENERIC_ARG_PID_FMT ",\"x\":%u,\"t\":%" PRIu64
                            ",\"h\":" GENERIC_ARG_PID_FMT ",\"f\":%lu}",
                            evln->pid, procidx, utn - context->start_time, argnfo_fork.cpid, argnfo.flags);
                assert(context->rev_fork_map.find(argnfo_fork.cpid) == context->rev_fork_map.end());
                context->rev_fork_map.insert(std::pair<upid_t, std::pair<upid_t, unsigned>>(
                    argnfo_fork.cpid, std::pair<upid_t, unsigned>(evln->pid, context->ve.size() - ve_init_size - 1)));
                n_processed += 2;
                context->event_count.fork++;
                srvec.push_back(
                    syscall_raw(pid, syscall_raw::SYS_FORK, argnfo_fork.cpid, argnfo.flags, utn - context->start_time));
            }
        } break;
        case PHASH_STR(Close): {
            struct CloseArgs argnfo = {};
            if (!parse_args(cmde + 1, SYSEVENT_CLOSE, &argnfo)) {
                fprintf(stderr, "ERROR: Failed to process args at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi, evln->pid,
                        evln->event_line);
                goto error;
            }
            uint64_t utn = (uint64_t)evln->timen;
            if ((context->total_event_count + n_processed > 0) && (context->rawoutfd))
                fprintf(context->rawoutfd, ",\n");
            if (context->rawoutfd)
                fprintf(context->rawoutfd,
                        "{\"c\":\"c\",\"p\":" GENERIC_ARG_PID_FMT ",\"x\":%u,\"t\":%" PRIu64 ",\"fd\":%d}", evln->pid,
                        procidx, utn - context->start_time, argnfo.fd);
            n_processed++;
            context->event_count.close++;
            srvec.push_back(syscall_raw(pid, syscall_raw::SYS_CLOSE, argnfo.fd, -1, 0, utn - context->start_time));
        } break;
        case PHASH_STR(Pipe): {
            struct PipeArgs argnfo = {};
            if (!parse_args(cmde + 1, SYSEVENT_PIPE, &argnfo)) {
                fprintf(stderr, "ERROR: Failed to process args at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi, evln->pid,
                        evln->event_line);
                goto error;
            }
            uint64_t utn = (uint64_t)evln->timen;
            if ((context->total_event_count + n_processed > 0) && (context->rawoutfd))
                fprintf(context->rawoutfd, ",\n");
            if (context->rawoutfd)
                fprintf(context->rawoutfd,
                        "{\"c\":\"p\",\"p\":" GENERIC_ARG_PID_FMT ",\"x\":%u,\"t\":%" PRIu64
                        ",\"fdr\":%d,\"fdw\":%d,\"f\":%lu}",
                        evln->pid, procidx, utn - context->start_time, argnfo.fd1, argnfo.fd2, argnfo.flags);
            n_processed++;
            context->event_count.pipe++;
            srvec.push_back(syscall_raw(pid, syscall_raw::SYS_PIPE, argnfo.fd1, argnfo.fd2, argnfo.flags,
                                        utn - context->start_time));
        } break;
        case PHASH_STR(Dup): {
            struct DupArgs argnfo = {};
            if (!parse_args(cmde + 1, SYSEVENT_DUP, &argnfo)) {
                fprintf(stderr, "ERROR: Failed to process args at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi, evln->pid,
                        evln->event_line);
                goto error;
            }
            uint64_t utn = (uint64_t)evln->timen;
            if ((context->total_event_count + n_processed > 0) && (context->rawoutfd))
                fprintf(context->rawoutfd, ",\n");
            if (context->rawoutfd)
                fprintf(context->rawoutfd,
                        "{\"c\":\"d\",\"p\":" GENERIC_ARG_PID_FMT ",\"x\":%u,\"t\":%" PRIu64
                        ",\"ofd\":%d,\"nfd\":%d,\"f\":%lu}",
                        evln->pid, procidx, utn - context->start_time, argnfo.oldfd, argnfo.newfd, argnfo.flags);
            n_processed++;
            context->event_count.dup++;
            srvec.push_back(syscall_raw(pid, syscall_raw::SYS_DUP, argnfo.oldfd, argnfo.newfd, argnfo.flags,
                                        utn - context->start_time));
        } break;
        case PHASH_STR(Open): {
            struct OpenArgs argnfo = {};
            if (!parse_args(cmde + 1, SYSEVENT_OPEN, &argnfo)) {
                fprintf(stderr, "ERROR: Failed to process args at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi, evln->pid,
                        evln->event_line);
                goto error;
            }
            /* Get open arguments */
            ++vi;
            char *longfn = get_longpath_arg(event_list, &vi, "FN", "FN_end\n");
            if (!longfn) {
                fprintf(stderr, "WARNING: Invalid open argument line [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                        evln->pid, VEC_ACCESS(*event_list, vi)->event_line);
                continue;
            }
            if (strlen(longfn) != argnfo.fnamesize) {
                fprintf(stderr,
                        "ERROR: Size mismatch in fname (%zu vs %zu) open argument [%lu]: [" GENERIC_ARG_PID_FMT
                        "] %s\n",
                        strlen(longfn), argnfo.fnamesize, vi, evln->pid, evln->event_line);
                goto error;
            }
            ++vi;
            char *longfo = get_longpath_arg(event_list, &vi, "FO", "FO_end\n");
            if (!longfo) {
                fprintf(stderr, "WARNING: Invalid open argument line [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                        evln->pid, VEC_ACCESS(*event_list, vi)->event_line);
                continue;
            }
            if (strlen(longfo) != argnfo.forigsize) {
                fprintf(stderr,
                        "ERROR: Size mismatch in fname (%zu vs %zu) open argument [%lu]: [" GENERIC_ARG_PID_FMT
                        "] %s\n",
                        strlen(longfo), argnfo.forigsize, vi, evln->pid, evln->event_line);
                goto error;
            }
            uint64_t utn = (uint64_t)evln->timen;
            MAKE_JSON_ESCAPED(_longfn, longfn);
            MAKE_JSON_ESCAPED(_longfo, longfo);
            struct parsed_entry &e = context->ve.back();
            e.addUpdateFile(_longfn, argnfo.flags, evln->pid, _longfo);
            if ((context->total_event_count + n_processed > 0) && (context->rawoutfd))
                fprintf(context->rawoutfd, ",\n");
            if (context->rawoutfd)
                fprintf(context->rawoutfd,
                        "{\"c\":\"o\",\"p\":" GENERIC_ARG_PID_FMT ",\"x\":%u,\"t\":%" PRIu64
                        ",\"h\":\"%s\",\"o\":\"%s\",\"f\":%lu,\"m\":%ld,\"fd\":%d}",
                        evln->pid, procidx, utn - context->start_time, _longfn, _longfo, argnfo.flags, argnfo.mode,
                        argnfo.fd);
            FREE_JSON_ESCAPED(_longfn);
            FREE_JSON_ESCAPED(_longfo);
            free((void *)longfn);
            free((void *)longfo);
            n_processed++;
            context->event_count.open++;
        } break;
        case PHASH_STR(RenameFrom):
        case PHASH_STR(Rename2From): {
            struct RenameArgs argnfo_RF = {};
            if (event == PHASH_STR(RenameFrom)) {
                if (!parse_args(cmde + 1, SYSEVENT_RENAMEFROM, &argnfo_RF)) {
                    fprintf(stderr, "ERROR: Failed to process args at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                            evln->pid, evln->event_line);
                    goto error;
                }
            } else {
                if (!parse_args(cmde + 1, SYSEVENT_RENAME2FROM, &argnfo_RF)) {
                    fprintf(stderr, "ERROR: Failed to process args at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                            evln->pid, evln->event_line);
                    goto error;
                }
            }
            /* Get rename arguments */
            ++vi;
            char *longfn_RF = get_longpath_arg(event_list, &vi, "RF", "RF_end\n");
            if (!longfn_RF) {
                fprintf(stderr, "WARNING: Invalid rename argument line [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                        evln->pid, VEC_ACCESS(*event_list, vi)->event_line);
                continue;
            }
            if (strlen(longfn_RF) != argnfo_RF.fnamesize) {
                fprintf(stderr,
                        "ERROR: Size mismatch in fname (%zu vs %zu) rename argument [%lu]: [" GENERIC_ARG_PID_FMT
                        "] %s\n",
                        strlen(longfn_RF), argnfo_RF.fnamesize, vi, evln->pid, evln->event_line);
                goto error;
            }
            /* Next event should be RenameTo or RenameFailed */
            ++vi;
            if (vi >= VEC_SIZE(*event_list))
                break;
            eventTuple_t *ievln = VEC_ACCESS(*event_list, vi);
            char *cmde = strchr(ievln->event_line, '|');
            if ((simple_hashn(ievln->event_line, cmde - ievln->event_line)) == PHASH_STR(RenameTo)) {
                struct RenameArgs argnfo_RT = {};
                if (!parse_args(cmde + 1, SYSEVENT_RENAMETO, &argnfo_RT)) {
                    fprintf(stderr, "ERROR: Failed to process args at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                            ievln->pid, ievln->event_line);
                    goto error;
                }
                /* Get rename arguments */
                ++vi;
                char *longfn_RT = get_longpath_arg(event_list, &vi, "RT", "RT_end\n");
                if (!longfn_RT) {
                    fprintf(stderr, "WARNING: Invalid rename argument line [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                            evln->pid, VEC_ACCESS(*event_list, vi)->event_line);
                    continue;
                }
                if (strlen(longfn_RT) != argnfo_RT.fnamesize) {
                    fprintf(stderr,
                            "ERROR: Size mismatch in fname (%zu vs %zu) rename argument [%lu]: [" GENERIC_ARG_PID_FMT
                            "] %s\n",
                            strlen(longfn_RF), argnfo_RF.fnamesize, vi, evln->pid, evln->event_line);
                    goto error;
                }
                uint64_t utn = (uint64_t)evln->timen;
                MAKE_JSON_ESCAPED(_longfn_RF, longfn_RF);
                MAKE_JSON_ESCAPED(_longfn_RT, longfn_RT);
                struct parsed_entry &e = context->ve.back();
                e.addUpdateFile(longfn_RF, O_RDONLY);
                e.addUpdateFile(longfn_RT, O_WRONLY);
                if ((context->total_event_count + n_processed > 0) && (context->rawoutfd))
                    fprintf(context->rawoutfd, ",\n");
                if (context->rawoutfd)
                    fprintf(context->rawoutfd,
                            "{\"c\":\"r\",\"p\":" GENERIC_ARG_PID_FMT ",\"x\":%u,\"t\":%" PRIu64
                            ",\"o\":\"%s\",\"n\":\"%s\",\"f\":%lu}",
                            evln->pid, procidx, utn - context->start_time, _longfn_RF, _longfn_RT, argnfo_RF.flags);
                FREE_JSON_ESCAPED(_longfn_RF);
                FREE_JSON_ESCAPED(_longfn_RT);
                free((void *)longfn_RF);
                free((void *)longfn_RT);
                n_processed += 2;
                context->event_count.rename++;
            } else if ((simple_hashn(ievln->event_line, cmde - ievln->event_line)) == PHASH_STR(RenameFailed)) {
                free((void *)longfn_RF);
            } else {
                fprintf(stderr,
                        "WARNING: Invalid event after Rename(2)From (" GENERIC_ARG_PID_FMT
                        ")[%lu]: [" GENERIC_ARG_PID_FMT "] %s\n",
                        pid, vi, ievln->pid, ievln->event_line);
                free((void *)longfn_RF);
                vi--;
                break;
            }
        } break;
        case PHASH_STR(RenameFailed): {
            /* Sometimes it's just happening */
            fprintf(stderr, "WARNING: Stray RenameFailed event at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi, evln->pid,
                    evln->event_line);
        } break;
        case PHASH_STR(LinkFailed): {
            /* Didn't happen but eventually it will */
            fprintf(stderr, "WARNING: Stray LinkFailed event at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi, evln->pid,
                    evln->event_line);
        } break;
        case PHASH_STR(LinkFrom):
        case PHASH_STR(LinkatFrom): {
            struct LinkArgs argnfo_LF = {};
            if (event == PHASH_STR(LinkFrom)) {
                if (!parse_args(cmde + 1, SYSEVENT_LINKFROM, &argnfo_LF)) {
                    fprintf(stderr, "ERROR: Failed to process args at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                            evln->pid, evln->event_line);
                    goto error;
                }
            } else {
                if (!parse_args(cmde + 1, SYSEVENT_LINKATFROM, &argnfo_LF)) {
                    fprintf(stderr, "ERROR: Failed to process args at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                            evln->pid, evln->event_line);
                    goto error;
                }
            }
            /* Get link arguments */
            ++vi;
            char *longfn_LF = get_longpath_arg(event_list, &vi, "LF", "LF_end\n");
            if (!longfn_LF) {
                fprintf(stderr, "WARNING: Invalid link argument line [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                        evln->pid, VEC_ACCESS(*event_list, vi)->event_line);
                continue;
            }
            if (strlen(longfn_LF) != argnfo_LF.fnamesize) {
                fprintf(stderr,
                        "ERROR: Size mismatch in fname (%zu vs %zu) link argument [%lu]: [" GENERIC_ARG_PID_FMT
                        "] %s\n",
                        strlen(longfn_LF), argnfo_LF.fnamesize, vi, evln->pid, evln->event_line);
                goto error;
            }
            /* Next event should be LinkTo or LinkFailed */
            ++vi;
            if (vi >= VEC_SIZE(*event_list))
                break;
            eventTuple_t *ievln = VEC_ACCESS(*event_list, vi);
            char *cmde = strchr(ievln->event_line, '|');
            if ((simple_hashn(ievln->event_line, cmde - ievln->event_line)) == PHASH_STR(LinkTo)) {
                struct LinkArgs argnfo_LT = {};
                if (!parse_args(cmde + 1, SYSEVENT_LINKTO, &argnfo_LT)) {
                    fprintf(stderr, "ERROR: Failed to process args at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                            ievln->pid, ievln->event_line);
                    goto error;
                }
                /* Get link arguments */
                ++vi;
                char *longfn_LT = get_longpath_arg(event_list, &vi, "LT", "LT_end\n");
                if (!longfn_LT) {
                    fprintf(stderr, "WARNING: Invalid link argument line [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                            evln->pid, VEC_ACCESS(*event_list, vi)->event_line);
                    continue;
                }
                if (strlen(longfn_LT) != argnfo_LT.fnamesize) {
                    fprintf(stderr,
                            "ERROR: Size mismatch in fname (%zu vs %zu) link argument [%lu]: [" GENERIC_ARG_PID_FMT
                            "] %s\n",
                            strlen(longfn_LT), argnfo_LT.fnamesize, vi, evln->pid, evln->event_line);
                    goto error;
                }
                uint64_t utn = (uint64_t)evln->timen;
                MAKE_JSON_ESCAPED(_longfn_LF, longfn_LF);
                MAKE_JSON_ESCAPED(_longfn_LT, longfn_LT);
                struct parsed_entry &e = context->ve.back();
                e.addUpdateFile(longfn_LF, O_RDONLY);
                e.addUpdateFile(longfn_LT, O_WRONLY);
                if ((context->total_event_count + n_processed > 0) && (context->rawoutfd))
                    fprintf(context->rawoutfd, ",\n");
                if (context->rawoutfd)
                    fprintf(context->rawoutfd,
                            "{\"c\":\"l\",\"p\":" GENERIC_ARG_PID_FMT ",\"x\":%u,\"t\":%" PRIu64
                            ",\"o\":\"%s\",\"n\":\"%s\",\"f\":%lu}",
                            evln->pid, procidx, utn - context->start_time, _longfn_LF, _longfn_LT, argnfo_LF.flags);
                FREE_JSON_ESCAPED(_longfn_LF);
                FREE_JSON_ESCAPED(_longfn_LT);
                free((void *)longfn_LF);
                free((void *)longfn_LT);
                n_processed += 2;
                context->event_count.link++;
            } else if ((simple_hashn(ievln->event_line, cmde - ievln->event_line)) == PHASH_STR(LinkFailed)) {
                free((void *)longfn_LF);
            } else {
                fprintf(stderr,
                        "WARNING: Invalid event after Link(At)From (" GENERIC_ARG_PID_FMT
                        ")[%lu]: [" GENERIC_ARG_PID_FMT "] %s\n",
                        pid, vi, ievln->pid, ievln->event_line);
                free((void *)longfn_LF);
                vi--;
                break;
            }
        } break;
        case PHASH_STR(Symlink): {
            struct SymLinkArgs argnfo = {};
            if (!parse_args(cmde + 1, SYSEVENT_SYMLINK, &argnfo)) {
                fprintf(stderr, "ERROR: Failed to process args at [%lu]: %s\n", vi, evln->event_line);
                goto error;
            }
            /* Get symlink arguments */
            ++vi;
            char *longfn_ST = get_longpath_arg(event_list, &vi, "ST", "ST_end\n");
            if (!longfn_ST) {
                fprintf(stderr, "WARNING: Invalid symlink argument line [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                        evln->pid, VEC_ACCESS(*event_list, vi)->event_line);
                continue;
            }
            if (strlen(longfn_ST) != argnfo.targetnamesize) {
                fprintf(
                    stderr,
                    "ERROR: Size mismatch in targetnamesize (%zu vs %zu) symlink argument [%lu]: [" GENERIC_ARG_PID_FMT
                    "] %s\n",
                    strlen(longfn_ST), argnfo.targetnamesize, vi, evln->pid, evln->event_line);
                goto error;
            }
            ++vi;
            char *longfn_SR = get_longpath_arg(event_list, &vi, "SR", "SR_end\n");
            if (longfn_SR) {
                if (strlen(longfn_ST) != argnfo.targetnamesize) {
                    fprintf(stderr,
                            "ERROR: Size mismatch in resolvednamesize (%zu vs %zu) symlink argument [%lu]: "
                            "[" GENERIC_ARG_PID_FMT "] %s\n",
                            strlen(longfn_SR), argnfo.resolvednamesize, vi, evln->pid, evln->event_line);
                    goto error;
                }
                ++vi;
            }
            char *longfn_SL = get_longpath_arg(event_list, &vi, "SL", "SL_end\n");
            if (!longfn_SL) {
                fprintf(stderr, "WARNING: Invalid symlink argument line [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi,
                        evln->pid, VEC_ACCESS(*event_list, vi)->event_line);
                continue;
            }
            if (strlen(longfn_SL) != argnfo.linknamesize) {
                fprintf(
                    stderr,
                    "ERROR: Size mismatch in linknamesize (%zu vs %zu) symlink argument [%lu]: [" GENERIC_ARG_PID_FMT
                    "] %s\n",
                    strlen(longfn_SL), argnfo.linknamesize, vi, evln->pid, evln->event_line);
                goto error;
            }
            uint64_t utn = (uint64_t)evln->timen;
            MAKE_JSON_ESCAPED(_longfn_ST, longfn_ST);
            MAKE_JSON_ESCAPED(_longfn_SR, longfn_SR);
            MAKE_JSON_ESCAPED(_longfn_SL, longfn_SL);
            struct parsed_entry &e = context->ve.back();
            if (_longfn_SR) {
                e.addUpdateFile(_longfn_SR, O_RDONLY);
            }
            e.addUpdateFile(longfn_SL, O_WRONLY);
            if ((context->total_event_count + n_processed > 0) && (context->rawoutfd))
                fprintf(context->rawoutfd, ",\n");
            if (context->rawoutfd) {
                if (_longfn_SR) {
                    fprintf(context->rawoutfd,
                            "{\"c\":\"s\",\"p\":" GENERIC_ARG_PID_FMT ",\"x\":%u,\"t\":%" PRIu64
                            ",\"o\":\"%s\",\"r\":\"%s\",\"n\":\"%s\"}",
                            evln->pid, procidx, utn - context->start_time, _longfn_ST, _longfn_SR, _longfn_SL);
                } else {
                    fprintf(context->rawoutfd,
                            "{\"c\":\"s\",\"p\":" GENERIC_ARG_PID_FMT ",\"x\":%u,\"t\":%" PRIu64
                            ",\"o\":\"%s\",\"n\":\"%s\"}",
                            evln->pid, procidx, utn - context->start_time, _longfn_ST, _longfn_SL);
                }
            }
            FREE_JSON_ESCAPED(_longfn_ST);
            FREE_JSON_ESCAPED(_longfn_SR);
            FREE_JSON_ESCAPED(_longfn_SL);
            free((void *)longfn_ST);
            free((void *)longfn_SR);
            free((void *)longfn_SL);
            n_processed += 2;
            context->event_count.symlink++;
        } break;
        case PHASH_STR(Exit): {
            // Nothing to get from it right now (later we might get process return code)
            uint64_t utn = (uint64_t)evln->timen;
            if (context->start_time == 0)
                context->start_time = utn;
            n_processed++;
            context->event_count.exit++;
            srvec.push_back(syscall_raw(pid, syscall_raw::SYS_EXIT, utn - context->start_time));
        } break;
        default: {
            /* Try to ignore it */
            fprintf(stderr, "WARNING: Invalid sys command at [%lu]: [" GENERIC_ARG_PID_FMT "] %s\n", vi, evln->pid,
                    evln->event_line);
            break;
        }
        }
        vi++;
    }

    context->ve.back().settime(end_time - exe_start_time);
    context->total_event_count += n_processed;
    context->total_execs_count += n_execs;
    context->process_count++;
    return n_processed;

error:
    /* Pretend as if this process exited immediately after the fork */
    context->ve.resize(ve_init_size + 1);
    context->ve.back().settime(end_time - exe_start_time);
    context->total_event_count += n_processed;
    context->process_count++;
    for (size_t u = 0; u < srvec.size() - srvec_init_size; ++u)
        srvec.pop_back();
    context->event_count.exit++;
    srvec.push_back(syscall_raw(pid, syscall_raw::SYS_EXIT, end_time - context->start_time));
    return n_processed;
}
