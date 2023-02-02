#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stddef.h>
#include <signal.h>
#include "rbtree.h"
#include "parser.h"
#include "utils.h"
#include <algorithm>
#include <deque>

static volatile int interrupt = 0;
static void intHandler(int v) {
    interrupt = 1;
}

#ifdef LIBRARY_BUILD
extern "C" {
	int parser_main(int argc, char** argv);
}
#endif

struct eventlist_node {
	struct rb_node node;
	upid_t pid;
	vpeventTuple_t* event_list;
};

struct cached_eventlist {
	vpeventTuple_t* event_list;
	upid_t pid;
};

struct flush_cache_entry {
	struct eventlist_node* evnode;
	unsigned long hitcount; /* When this value is reached the cache is flushed */
	flush_cache_entry(struct eventlist_node* evnode, unsigned long hitcount): evnode(evnode), hitcount(hitcount) {}
};

static int eventlist_insert(struct rb_root* root, upid_t pid, vpeventTuple_t* evlst) {

	struct eventlist_node* data = (struct eventlist_node*)calloc(1,sizeof(struct eventlist_node));
	data->pid = pid;
	data->event_list = evlst;
	struct rb_node **_new = &(root->rb_node), *parent = 0;

	/* Figure out where to put new node */
	while (*_new) {
		struct eventlist_node* _this = container_of(*_new, struct eventlist_node, node);

		parent = *_new;
		if (data->pid<_this->pid)
			_new = &((*_new)->rb_left);
		else if (data->pid>_this->pid)
			_new = &((*_new)->rb_right);
		else {
		    free(data);
		    return 0;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, _new);
	rb_insert_color(&data->node, root);

	return 1;
}

static struct eventlist_node* eventlist_search(struct rb_root* root, upid_t pid) {

	struct rb_node *node = root->rb_node;

	while (node) {
		struct eventlist_node* data = container_of(node, struct eventlist_node, node);

		if (pid<data->pid) {
			node = node->rb_left;
		}
		else if (pid>data->pid) {
			node = node->rb_right;
		}
		else
			return data;
	}

	return 0;
}

static int pipe_open_for_write(struct fdmap_node* from, struct fdmap_node* to, int stdpipe = 0) {

	std::set<uint32_t> pms;
	for (auto u=from->fdmap.begin(); u!=from->fdmap.end(); ++u) {
		/* if stdpipe is 0 we don't care about the fd value */
		if (((*u).second.pipewr)&&(((*u).first==1)||(!stdpipe))) {
			pms.insert((*u).second.pipe_mark);
		}
	}
	for (auto u=to->fdmap.begin(); u!=to->fdmap.end(); ++u) {
		if (((*u).second.piperd)&&(((*u).first==0)||(!stdpipe))) {
			if (pms.find((*u).second.pipe_mark)!=pms.end()) {
				return 1;
			}
		}
	}
	return 0;
}

void help() {
	printf("Usage: etrace_parser <tracer_db_path> <parsed_db_path> [-r [<parsed_raw_db_path]]\n");
	printf("where:\n");
	printf("       <tracer_db_path>: by default it is '.nfsdb' file in the current directory\n");
	printf("       <parsed_db_path>: by default it is '.nfsdb.json' file in the current directory\n");
	printf("       -r: when this option is passed, along the parsed database also specified raw parsed JSON file is created ('.nfsdb.raw.json' file by default)\n");
}

static inline int eventTupleTimeLess(eventTuple_t* a, eventTuple_t* b) {
	if (a->time < b->time) return 1;
	if (a->time > b->time) return 0;
	if (a->timen < b->timen) return 1;
	return 0;
}

static inline int compare_peventTuple_t (const void* a, const void* b) {

	eventTuple_t* eTa = *((eventTuple_t**)a);
	eventTuple_t* eTb = *((eventTuple_t**)b);

	if (eventTupleTimeLess(eTa,eTb)) return -1;
	if ((eTa->time==eTb->time) && (eTa->timen==eTb->timen)) return 0;
	return 1;
}

int parser_main(int argc, char** argv) {

	if (argc>=2) {
		if (!strcmp(argv[1],"-h") || !strcmp(argv[1],"--help")) {
			help();
			return 0;
		}
	}

	const char* dbpath;
	if (argc>=2) {
		dbpath = argv[1];
	}
	else {
		dbpath = ".nfsdb";
	}

	const char* outpath;
	if (argc>=3) {
		outpath = argv[2];
	}
	else {
		outpath = ".nfsdb.json";
	}

	const char* rawpath = 0;
	if (argc>=4) {
		if (!strcmp(argv[3],"-r")) {
			if (argc>=5) {
				rawpath = argv[4];
			}
			else {
				rawpath = ".nfsdb.raw.json";
			}
		}
	}

	struct parse_context context = {};
	ssize_t nlcount = count_file_lines(dbpath);
	if (nlcount<0) {
		return nlcount;
	}

	if (rawpath) {
		context.rawoutfd = fopen(rawpath,"w");
		if (!context.rawoutfd) {
			printf("Failed to open %s for writing: %d\n",rawpath,errno);
			return EXIT_FAILURE;
		}
	}
	context.outfd = fopen(outpath,"w");
	if (!context.outfd) {
		printf("Failed to open %s for writing: %d\n",outpath,errno);
		return EXIT_FAILURE;
	}

	FILE* fd = fopen(dbpath,"rb");
	if (!fd) {
		printf("Failed to open %s for reading: %d\n",dbpath,errno);
		return ENOENT;
	}

	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	unsigned long lineno = 0;
	const static int print_stats = 1;

	struct rb_root event_map_root = RB_ROOT;

	struct sigaction act;
	act.sa_handler = intHandler;
	sigaction(SIGINT, &act, 0);
	interrupt = 0;

	printf("Parsing events ...\n");
	printf("0%%");
	fflush(stdout);
	peventTuple_t evln = 0;
	long multilines = 0;

	if (context.rawoutfd) fprintf(context.rawoutfd,"[");
	fprintf(context.outfd,"[");
	struct cached_eventlist cached_event_list = {0,-1};

	std::deque<flush_cache_entry> flush_cache;
	uint64_t last_event_time = 0;
	while ((read = getline(&line, &len, fd)) != -1) {
		if (lineno++ == 0) {
			/* Ignore first line (INITCWD) */
			continue;
		}
		if ((lineno % (nlcount/100))==0) {
			printf("\r%lu%%",lineno / (nlcount/100));
			fflush(stdout);
		}
		if ((strlen(line)<4)||((line[0]!='0')||(line[1]!=':')||(line[2]!=' '))) {
			fprintf(stderr,"ERROR: Invalid format for log line [%lu]: %s\n",lineno,line);
			return EXIT_FAILURE;
		}
		else {
			evln = TYPE_CREATE(eventTuple_t);
			if (!parse_generic_args(line+3,evln)) {
				fprintf(stderr,"ERROR: Cannot parse generic args [%lu]: %s\n",lineno,line);
				return EXIT_FAILURE;
			}
		}
		last_event_time = (uint64_t)evln->time*(uint64_t)1000000000UL+evln->timen;
		if (context.start_time==0) {
			context.start_time=last_event_time;
		}
		if (context.root_pid<=0) {
			context.root_pid = evln->pid;
		}
		vpeventTuple_t* event_list = 0;
		/* @15:06
		 * Most of the time the next event will come from the same process as the previous. Cache it. */
		if (cached_event_list.pid==evln->pid) {
			event_list = cached_event_list.event_list;
		}
		else {
			struct eventlist_node* evnode = eventlist_search(&event_map_root,evln->pid);
			if (!evnode) {
				/* Insert empty event_list */
				event_list = TYPE_CREATE(vpeventTuple_t);
				VEC_INIT(*event_list);
				eventlist_insert(&event_map_root,evln->pid,event_list);
			}
			else {
				event_list = evnode->event_list;
			}
			cached_event_list.pid = evln->pid;
			cached_event_list.event_list = event_list;
			/* @15:30
			 * It turned out that caching the pid doesn't matter at all... */
		}
		VEC_APPEND(eventTuple_t*,*event_list,evln);
		/* Handle special cases */
		if (!strncmp(evln->event_line,"Cont",4)) {
			if (evln->event_line[4]=='|') {
				VEC_POPBACK(*event_list);
				assert(VEC_SIZE(*event_list)>0);
				eventTuple_t* prev_evln = VEC_BACK(*event_list);
				prev_evln->event_line = strappend(prev_evln->event_line,&evln->event_line[5]);
			}
			else if (!strncmp(&evln->event_line[4],"_end",4)) {
				VEC_POPBACK(*event_list);
				multilines++;
			}
			else {
				fprintf(stderr,"ERROR: Invalid format for log line [%lu]: %s\n",lineno,line);
				return EXIT_FAILURE;
			}
			free((void*)evln->event_line);
			free((void*)evln);
		}
		else if (!strncmp(evln->event_line,"Exit",4)) {
			/* It turns out that in some cases when processes are migrated across processors there might be events in the log
			 * for a given process *after* the 'Exit' event. Delay the parsing upon 'Exit' for some predefined amount of events. */
			struct eventlist_node* evnode = eventlist_search(&event_map_root,evln->pid);
			assert(evnode);
#if EVENT_FLUSH_CACHE_MARGIN>0
			flush_cache.push_back(flush_cache_entry(evnode,lineno+EVENT_FLUSH_CACHE_MARGIN));
#else
			long evcount;
			if ((evcount=parse_write_process_events(evln->pid,evnode->event_list,&context))<0) {
				fprintf(stderr,"ERROR: Failed to parse process events for (" GENERIC_ARG_PID_FMT ")\n",evln->pid);
				return EXIT_FAILURE;
			}
			/* Now get rid of the map entry */
			for (unsigned long vi=0; vi<VEC_SIZE(*evnode->event_list); ++vi) {
				eventTuple_t* evt = VEC_ACCESS(*evnode->event_list,vi);
				free((void*)evt->event_line);
				free((void*)evt);
			}
			VEC_DESTROY(*evnode->event_list);
			free((void*)evnode->event_list);
			rb_erase(&evnode->node, &event_map_root);
			free(evnode);
#endif
		}
#if EVENT_FLUSH_CACHE_MARGIN>0
		if (flush_cache.front().hitcount==lineno) {
			struct eventlist_node* evnode = flush_cache.front().evnode;
			flush_cache.pop_front();
			long evcount;
			/* Sort the event list by the start time (sometimes events gets intermingled when process is migrated across processors )*/
			qsort(evnode->event_list->a,VEC_SIZE(*evnode->event_list),sizeof(eventTuple_t*),compare_peventTuple_t);
			eventTuple_t* start = VEC_ACCESS(*evnode->event_list,0);
			eventTuple_t* last = VEC_ACCESS(*evnode->event_list,VEC_SIZE(*evnode->event_list)-1);
			uint64_t start_time = (uint64_t)start->time*(uint64_t)1000000000UL+start->timen;
			uint64_t end_time = (uint64_t)last->time*(uint64_t)1000000000UL+last->timen;
			if ((evcount=parse_write_process_events(evnode->pid,evnode->event_list,&context,start_time,end_time))<0) {
				fprintf(stderr,"ERROR: Failed to parse process events for (" GENERIC_ARG_PID_FMT ")\n",evnode->pid);
				return EXIT_FAILURE;
			}
			/* Now get rid of the map entry */
			for (unsigned long vi=0; vi<VEC_SIZE(*evnode->event_list); ++vi) {
				eventTuple_t* evt = VEC_ACCESS(*evnode->event_list,vi);
				free((void*)evt->event_line);
				free((void*)evt);
			}
			VEC_DESTROY(*evnode->event_list);
			free((void*)evnode->event_list);
			rb_erase(&evnode->node, &event_map_root);
			free(evnode);
		}
#endif
		if (interrupt) break;
	}
	if (!interrupt) printf("\r100%%\n"); else printf("\n");
	if (interrupt) exit(2);

    long procs_at_exit = 0;
	struct rb_node * p = rb_first(&event_map_root);
    while(p) {
        struct eventlist_node* evnode = (struct eventlist_node*)p;
        qsort(evnode->event_list->a,VEC_SIZE(*evnode->event_list),sizeof(eventTuple_t*),compare_peventTuple_t);
		eventTuple_t* start = VEC_ACCESS(*evnode->event_list,0);
		uint64_t start_time = (uint64_t)start->time*(uint64_t)1000000000UL+start->timen;
        long evcount;
		if ((evcount = parse_write_process_events(evnode->pid,evnode->event_list,&context,start_time,last_event_time))<0) {
			fprintf(stderr,"ERROR: Failed to parse process events for (" GENERIC_ARG_PID_FMT ")\n",evnode->pid);
			return EXIT_FAILURE;
		}
        rb_erase(p, &event_map_root);
        p = rb_next(p);
        for (unsigned long vi=0; vi<VEC_SIZE(*evnode->event_list); ++vi) {
        	eventTuple_t* evt = VEC_ACCESS(*evnode->event_list,vi);
        	free((void*)evt->event_line);
        	free((void*)evt);
		}
		VEC_DESTROY(*evnode->event_list);
		free((void*)evnode->event_list);
        /* Add artificial Exit event */
        if ((evcount>0)&&(context.rawoutfd))  fprintf(context.rawoutfd,",\n");
        if (context.rawoutfd) fprintf(context.rawoutfd,"{\"c\":\"x\",\"p\": " GENERIC_ARG_PID_FMT ",\"t\":%lu}", evnode->pid,0UL);
        free((void*)evnode);
        procs_at_exit++;
    }

    /* Check if we have some child processes without any parsed entries (no syscalls called in child whatsoever)
     * In such case add artificial empty execution
     */
    unsigned long empty_child_count = 0;
    for (std::map<upid_t,std::pair<upid_t,unsigned>>::iterator i=context.rev_fork_map.begin(); i!=context.rev_fork_map.end(); ++i) {
    	if (context.pset.find((*i).first)==context.pset.end()) {
    		empty_child_count++;
    	}
    }

   	if (print_stats) {
   		printf("processes: %ld\n",context.process_count);
   		printf("total event count: %ld\n",context.total_event_count);
   		printf("  exec: %ld\n",context.event_count.exec);
   		printf("  fork: %ld\n",context.event_count.fork);
   		printf("  close: %ld\n",context.event_count.close);
   		printf("  open: %ld\n",context.event_count.open);
   		printf("  pipe: %ld\n",context.event_count.pipe);
   		printf("  dup: %ld\n",context.event_count.dup);
   		printf("  rename: %ld\n",context.event_count.rename);
   		printf("  link: %ld\n",context.event_count.link);
   		printf("  symlink: %ld\n",context.event_count.symlink);
   		printf("  exit: %ld\n",context.event_count.exit);
   		printf("multilines: %ld\n",multilines);
   		printf("written rw entries: %ld\n",context.total_rw_count);
   		printf("written fork entries: %ld\n",context.total_fork_count);
   		printf("procs_at_exit: %ld\n",procs_at_exit);
   		printf("empty child processes: %ld\n",empty_child_count);
   	}

   	if (context.rawoutfd) fprintf(context.rawoutfd,"]");

    fclose(fd);
    if (context.rawoutfd) fclose(context.rawoutfd);
    free(line);
    if (interrupt) exit(2);

    /* Now create the pipe map
     *  Generally processes can communicate with each other using pipes. What do we want to achieve
     *  (for the purpose of dependency computation) is to know whether any child process could
     *  pass any data to the parent process.
     *  When a process executes pipe it creates two file descriptors (Nr,Mw).
     *  When it forks the child process inherits both descriptors (Nr,Mw).
     *  Now communication is possible when both file descriptors (for read and write) are open in the process
     *  at the time of cloning.
     *  The thing is the child will eventually call execve syscall and the process address space will be reset (and some
     *  file descriptors could be closed as well depending on the CLOEXEC flag) so we should probably focus on possibility
     *  of communication between processes after the execve (which should reduce the false positive rate, i.e. there is
     *  communication possibility considered during dependency computation whereas no information was really possible to send).
     *  The pipe map will map any specific process into its parent for which there was an open pipe for writing
     *  from this process to the parent at some point in time.
     *  Now the problem is that the root process can create a pipe and spawn several children (which can spawn another
     *  children and so on). In this scenario each process can talk with any other process using the same file descriptors.
     *  We handle that by assigning a unique value to each file descriptor that originated from pipe to find opened
     *  ends of a given pipe in different processes.
     *  Let's see an example from real life.
     *  `gcc` process calls a pipe creating two file descriptors => (3r,4w) and then forks and execve `cc1`.
     *  `cc1` duplicates 4w => 1w and closes (3r,4w). We're left with 1w in `cc1`.
     *  `gcc` forks again and execve `as` (we still have (3r,4w).
     *  `as` duplicates 3r => 0r and closes (3r,4w). We're left with 0r in `as`.
     *  This way we can write to stdout in `cc1` and read that back in `as` from stdin.
     *  Ok, so how we'll do it?
     *   We will track file descriptors created by PIPE in all processes. Whenever we got the EXECVE syscall we compare
     *   the file descriptor tables of the parent and the current process after EXECVE. If it's possible to write to
     *   the parent using the same 'pipe_mark' file descriptors in both processes (write end in the child and read end
     *   in the parent) we will add it to the 'pipe_mark'.
     *   Theoretically we should walk the process tree and verify all the parents along the way but hey who sufficiently
     *   sane sends data through pipe to its great grandparents? In entire Linux kernel build for Pixel 5 we add around
     *   10% new entries to the pipe_map allowing this.
     *   The same needs to be done for siblings. There's a problem though. There might be processes which spawn thousands
     *   of children (root make or ninja process for example). Whenever new child is born we would need to compare its file
     *   descriptor table with all the siblings. We can do this only for existing siblings at the time of new exec to somehow
     *   alleviate this issue. Even with that there's still large number of superfluous connections between siblings
     *   (which can ultimately lead to cycles). To further simplify this issue consider only connections that come
     *   from stdout (1) to stdin (0) between siblings.
     */

    printf("Creating pipe map...\n");
    printf("0%%");
    fflush(stdout);

    pipe_map_t pipe_map;
    std::map<upid_t,unsigned> exeIdxMap;
    std::map<upid_t,std::set<upid_t>> fork_map;
    std::map<upid_t,upid_t> rev_fork_map;
    syscall_raw *root_sys = nullptr;
    std::map<upid_t,fdmap_node*> fdmap_process_map;
    fdmap_node *root_fdmap_node = nullptr;

    std::sort(context.srvec.begin(), context.srvec.end(), syscall_raw_sort_key());
    /* Create root file descriptor map */
    if (context.srvec.size() > 0) {
        root_sys = context.srvec.data();
        root_fdmap_node = new fdmap_node(root_sys->pid);
        fdmap_process_map.insert(std::pair<upid_t,fdmap_node*>(root_sys->pid,root_fdmap_node));
    }

    /* Create dummy 'init' process */
    fork_map.insert(std::pair<upid_t,std::set<upid_t>>(0,std::set<upid_t>()));
#ifdef ENABLE_PARENT_PIPE_CHECK
    unsigned long parent_pipe_count = 0;
#endif

    uint32_t pipe_index=0;
    for (auto i=context.srvec.begin(); i!=context.srvec.end(); ++i) {
        if (context.srvec.size()>=100) {
            if ((std::distance(context.srvec.begin(),i) % (context.srvec.size()/100))==0) {
                printf("\r%lu%%",std::distance(context.srvec.begin(),i) / (context.srvec.size()/100));
                fflush(stdout);
            }
        }
    	syscall_raw& sys = (*i);
    	if (sys.sysname==syscall_raw::SYS_PIPE) {
			if (fdmap_process_map.find(sys.pid)==fdmap_process_map.end()) {
				printf("WARNING: no file descriptor table for process (" GENERIC_ARG_PID_FMT ") at PIPE [%zu|%lu]\n",sys.pid,
						std::distance(context.srvec.begin(),i),sys.start_time);
				continue;
			}
			fdmap_node* fdmap = fdmap_process_map[sys.pid];
			fdmap->fdmap.insert(std::pair<int,fdinfo>(sys.i0,fdinfo(1,0,(sys.ul&O_CLOEXEC)!=0,pipe_index)));
			fdmap->fdmap.insert(std::pair<int,fdinfo>(sys.i1,fdinfo(0,1,(sys.ul&O_CLOEXEC)!=0,pipe_index)));
			pipe_index++;
		}
		else if (sys.sysname==syscall_raw::SYS_DUP) {
			if (fdmap_process_map.find(sys.pid)==fdmap_process_map.end()) {
				printf("WARNING: no file descriptor table for process (" GENERIC_ARG_PID_FMT ") at DUP [%zu|%lu]\n",sys.pid,
						std::distance(context.srvec.begin(),i),sys.start_time);
				continue;
			}
			fdmap_node* fdmap = fdmap_process_map[sys.pid];
			if (fdmap->fdmap.find(sys.i0)==fdmap->fdmap.end()) {
				/* We can duplicate fd which didn't originate from PIPE (and therefore isn't tracked) */
				continue;
			}
			struct fdinfo& old_fdinfo = fdmap->fdmap[sys.i0];
			if (fdmap->fdmap.find(sys.i1)!=fdmap->fdmap.end()) {
				/* Silently close the destination fd */
				fdmap->fdmap.erase(sys.i1);
			}
			fdmap->fdmap.insert(std::pair<int,fdinfo>(sys.i1,
					fdinfo(old_fdinfo.piperd,old_fdinfo.pipewr,(sys.ul&O_CLOEXEC)!=0,old_fdinfo.pipe_mark)));
		}
		else if (sys.sysname==syscall_raw::SYS_FORK) {
			exeIdxMap.insert(std::pair<upid_t,unsigned>(sys.pv,0));
			if (fdmap_process_map.find(sys.pid)==fdmap_process_map.end()) {
				printf("WARNING: no file descriptor table for process (" GENERIC_ARG_PID_FMT ") at FORK [%zu|%lu]\n",sys.pid,
						std::distance(context.srvec.begin(),i),sys.start_time);
				continue;
			}
			fdmap_node* fdmap = fdmap_process_map[sys.pid];
			if (fdmap_process_map.find(sys.pv)!=fdmap_process_map.end()) {
				printf("WARNING: file descriptor map for new process (" GENERIC_ARG_PID_FMT ") spawned at (" GENERIC_ARG_PID_FMT
						") at FORK already in map [%zu|%lu]\n",
						sys.pv,sys.pid,std::distance(context.srvec.begin(),i),sys.start_time);
				continue;
			}
			if (sys.ul&CLONE_FILES) {
				/* Share the file descriptor table */
				fdmap_process_map.insert(std::pair<upid_t,fdmap_node*>(sys.pv,fdmap));
				fdmap->refs.insert(sys.pv);
			}
			else {
				/* Copy the file descriptor table */
				fdmap_node* new_fdmap_node = new fdmap_node(sys.pv);
				fdmap_process_map.insert(std::pair<upid_t,fdmap_node*>(sys.pv,new_fdmap_node));
				for (auto u=fdmap->fdmap.begin(); u!=fdmap->fdmap.end(); ++u) {
					new_fdmap_node->fdmap.insert(std::pair<int,fdinfo>((*u).first,
							fdinfo((*u).second.piperd,(*u).second.pipewr,(*u).second.cloexec,(*u).second.pipe_mark)));
				}
			}
			if (rev_fork_map.find(sys.pv)!=rev_fork_map.end()) {
				printf("WARNINIG: parent process entry for process (" GENERIC_ARG_PID_FMT ") spawned at (" GENERIC_ARG_PID_FMT
						") at FORK already in reverse process map [%zu|%lu]\n",
						sys.pv,sys.pid,std::distance(context.srvec.begin(),i),sys.start_time);
				continue;
			}
			rev_fork_map.insert(std::pair<upid_t,upid_t>(sys.pv,sys.pid));
			if (fork_map.find(sys.pid)==fork_map.end()) {
				fork_map.insert(std::pair<upid_t,std::set<upid_t>>(sys.pid,std::set<upid_t>()));
			}
			fork_map[sys.pid].insert(sys.pv);
		}
		else if (sys.sysname==syscall_raw::SYS_EXEC) {
			if (exeIdxMap.find(sys.pid)==exeIdxMap.end()) {
				exeIdxMap.insert(std::pair<upid_t,unsigned>(sys.pid,1));
			}
			else {
				exeIdxMap[sys.pid]++;
			}
			if (fdmap_process_map.find(sys.pid)==fdmap_process_map.end()) {
				printf("WARNING: no file descriptor table for process (" GENERIC_ARG_PID_FMT ") at EXECVE [%zu|%lu]\n",sys.pid,
						std::distance(context.srvec.begin(),i),sys.start_time);
				continue;
			}
			fdmap_node* fdmap = fdmap_process_map[sys.pid];
			/* Unshare file descriptor map (if it was shared between processes) */
			if (fdmap->refs.size()>1) {
				fdmap_node* new_fdmap_node = new fdmap_node(sys.pid);
				for (auto u=fdmap->fdmap.begin(); u!=fdmap->fdmap.end(); ++u) {
					new_fdmap_node->fdmap.insert(std::pair<int,fdinfo>((*u).first,
							fdinfo((*u).second.piperd,(*u).second.pipewr,(*u).second.cloexec,(*u).second.pipe_mark)));
				}
				if (fdmap->refs.find(sys.pid)==fdmap->refs.end()) {
					printf("ERROR: missing identifier for shared file descriptor table for process (" GENERIC_ARG_PID_FMT
							") at EXECVE [%zu|%lu]\n",sys.pid,std::distance(context.srvec.begin(),i),sys.start_time);
					exit(1);
				}
				fdmap->refs.erase(sys.pid);
				fdmap_process_map[sys.pid] = new_fdmap_node;

			}
			fdmap = fdmap_process_map[sys.pid];
			/* Close file descriptors marked as CLOEXEC */
			std::set<int> to_close;
			for (auto u=fdmap->fdmap.begin(); u!=fdmap->fdmap.end(); ++u) {
				if ((*u).second.cloexec) {
					to_close.insert((*u).first);
				}
			}
			for (auto u=to_close.begin(); u!=to_close.end(); ++u) {
				fdmap->fdmap.erase(*u);
			}
			if (rev_fork_map.find(sys.pid)==rev_fork_map.end()) {
				printf("WARNING: could not find active parent for process (" GENERIC_ARG_PID_FMT ") at EXECVE [%zu|%lu]\n",sys.pid,
						std::distance(context.srvec.begin(),i),sys.start_time);
				continue;
			}
			upid_t ppid = rev_fork_map[sys.pid];
			if (fdmap_process_map.find(ppid)==fdmap_process_map.end()) {
				printf("WARNING: no file descriptor table for process (" GENERIC_ARG_PID_FMT ") at EXECVE [%zu|%lu]\n",ppid,
						std::distance(context.srvec.begin(),i),sys.start_time);
				continue;
			}
			fdmap_node* parent_fdmap = fdmap_process_map[ppid];
			/* Look for write fd in the current process and read fd in the parent with the same 'pipe_mark' */
			if (pipe_open_for_write(fdmap,parent_fdmap)) {
				update_pipe_map(pipe_map,sys.pid,exeIdxMap[sys.pid],ppid,exeIdxMap[ppid]);
			}
#ifdef ENABLE_PARENT_PIPE_CHECK
			/* Check the parents? */
			while(rev_fork_map.find(ppid)!=rev_fork_map.end()) {
				upid_t nppid = rev_fork_map[ppid];
				if (fdmap_process_map.find(nppid)!=fdmap_process_map.end()) {
					fdmap_node* nparent_fdmap = fdmap_process_map[nppid];
					if (pipe_open_for_write(fdmap,nparent_fdmap)) {
						parent_pipe_count++;
						update_pipe_map(pipe_map,sys.pid,nppid);
					}
				}
				ppid = nppid;
			}
#endif
#ifdef ENABLE_SIBLING_PIPE_CHECK
			/* Now the siblings */
			if (fork_map.find(ppid)==fork_map.end()) {
				printf("WARNING: no entry in process map for parent process (" GENERIC_ARG_PID_FMT ") at EXECVE [%zu|%lu]\n",ppid,
						std::distance(context.srvec.begin(),i),sys.start_time);
				continue;
			}
			std::set<upid_t>& siblings_with_me = fork_map[ppid];
			for (auto u=siblings_with_me.begin(); u!=siblings_with_me.end(); ++u) {
				if (*u!=sys.pid) {
					if (fdmap_process_map.find(*u)!=fdmap_process_map.end()) {
						fdmap_node* sibling_fdmap = fdmap_process_map[*u];
						if (pipe_open_for_write(fdmap,sibling_fdmap,1)) {
							update_pipe_map(pipe_map,sys.pid,exeIdxMap[sys.pid],*u,exeIdxMap[*u]);
						}
						if (pipe_open_for_write(sibling_fdmap,fdmap,1)) {
							update_pipe_map(pipe_map,*u,exeIdxMap[*u],sys.pid,exeIdxMap[sys.pid]);
						}
					}
				}
			}
#endif

		}
		else if (sys.sysname==syscall_raw::SYS_CLOSE) {
			if (fdmap_process_map.find(sys.pid)==fdmap_process_map.end()) {
				printf("WARNING: no file descriptor table for process (" GENERIC_ARG_PID_FMT ") at CLOSE [%zu|%lu]\n",sys.pid,
						std::distance(context.srvec.begin(),i),sys.start_time);
				continue;
			}
			fdmap_node* fdmap = fdmap_process_map[sys.pid];
			if (fdmap->fdmap.find(sys.i0)!=fdmap->fdmap.end()) {
				fdmap->fdmap.erase(sys.i0);
			}
			else {
				/* We can close fd which didn't originate from PIPE (and therefore isn't tracked) */
			}
		}
		else if (sys.sysname==syscall_raw::SYS_EXIT) {
			if (rev_fork_map.find(sys.pid)==rev_fork_map.end()) {
				printf("WARNING: no entry in reverse process map for process (" GENERIC_ARG_PID_FMT ") at EXIT [%zu|%lu]\n",sys.pid,
						std::distance(context.srvec.begin(),i),sys.start_time);
				continue;
			}
			/* Remove this process from its parent children */
			upid_t ppid = rev_fork_map[sys.pid];
			if (fork_map.find(ppid)==fork_map.end()) {
				printf("WARNING: no entry in process map for parent process (" GENERIC_ARG_PID_FMT ") at EXIT [%zu|%lu]\n",ppid,
						std::distance(context.srvec.begin(),i),sys.start_time);
			}
			else {
				std::set<upid_t>& pchlds = fork_map[ppid];
				pchlds.erase(sys.pid);
			}
			rev_fork_map.erase(sys.pid);
			if (fork_map.find(sys.pid)==fork_map.end()) {
				/* No children */
				continue;
			}
			std::set<upid_t>& chlds = fork_map[sys.pid];
			if (chlds.size()>0) {
				/* Process exits but some children still active; pass it to 'init' process */
				for (auto u=chlds.begin(); u!=chlds.end(); ++u) {
					rev_fork_map[*u] = 0;
				}
				std::set<upid_t>& root_chlds = fork_map[0];
				root_chlds.insert(chlds.begin(),chlds.end());
			}
			if (fdmap_process_map.find(sys.pid)==fdmap_process_map.end()) {
				printf("WARNING: no file descriptor table for process (" GENERIC_ARG_PID_FMT ") at EXIT [%zu|%lu]\n",sys.pid,
						std::distance(context.srvec.begin(),i),sys.start_time);
				continue;
			}
			fdmap_node* fdmap = fdmap_process_map[sys.pid];
			if (fdmap->refs.size()>1) {
				if (fdmap->refs.find(sys.pid)==fdmap->refs.end()) {
					printf("ERROR: missing identifier for shared file descriptor table for process (" GENERIC_ARG_PID_FMT ") at EXIT [%zu|%lu]\n",
							sys.pid,std::distance(context.srvec.begin(),i),sys.start_time);
					exit(1);
				}
				fdmap->refs.erase(sys.pid);
			}
			else {
				delete fdmap;
			}
			fdmap_process_map.erase(sys.pid);
			fork_map.erase(sys.pid);
		}
    	if (interrupt) break;
    } /* for(i) */
    if (!interrupt) printf("\r100%%\n"); else printf("\n");
    if (interrupt) exit(2);
    for (auto u=fdmap_process_map.begin(); u!=fdmap_process_map.end(); ++u) {
    	if ((*u).second->refs.size()>1) {
    		if ((*u).second->refs.find((*u).first)==(*u).second->refs.end()) {
				printf("ERROR: missing identifier for shared file descriptor table for process (" GENERIC_ARG_PID_FMT ") at destructor\n",(*u).first);
				exit(1);
			}
    		(*u).second->refs.erase((*u).first);
    	}
    	else {
    		delete (*u).second;
    	}
    }
    unsigned long pipe_map_entries = 0;
    for (auto u=pipe_map.begin(); u!=pipe_map.end(); ++u) {
    	pipe_map_entries+=(*u).second.size();
    }
    printf("pipe_map size at exit: %zu keys, %lu entries total\n",pipe_map.size(),pipe_map_entries);
#ifdef ENABLE_PARENT_PIPE_CHECK
    printf("parent pipe count: %lu\n",parent_pipe_count);
#endif

    printf("Flushing entries...\n");
    printf("0%%");
    fflush(stdout);
    std::sort(context.ve.begin(), context.ve.end(), parsed_entry_sort_key());
    /* Now update parent pid information in parsed entries and flush the entries */
    for (auto i=context.ve.begin(); i!=context.ve.end(); ++i) {
		if ((context.ve.size()>=100)&&((std::distance(context.ve.begin(),i) % (context.ve.size()/100))==0)) {
			printf("\r%lu%%",std::distance(context.ve.begin(),i) / (context.ve.size()/100));
			fflush(stdout);
		}
		struct parsed_entry& e = *i;
		if (context.rev_fork_map.find(e.pid)!=context.rev_fork_map.end()) {
			e.parent = context.rev_fork_map[e.pid];
		}
		else {
			e.parent = std::pair<upid_t,unsigned>(-1,0);
		}
		if (i==context.ve.begin()) {
			fprintf(context.outfd,"\n");
		}
		else {
			fprintf(context.outfd,",\n");
		}
		// Print the entry
		auto rwi = e.rwmap.begin();
print_entry:
		fprintf(context.outfd,"{\"p\":" GENERIC_ARG_PID_FMT ",\"x\":%u,\"e\":%lu,\"b\":\"%s\",\"w\":\"%s\",\"v\":[",
				e.pid,e.procidx,e.etime,e.progname_p.c_str(),e.cwd.c_str());
		for (auto u=e.argv.begin(); u!=e.argv.end(); ++u) {
			fprintf(context.outfd,"\"%s\"",(*u).c_str());
			if (u!=e.argv.end()-1) fprintf(context.outfd,",");
		}
		fprintf(context.outfd,"],\"c\":[");
		for (auto u=e.vchild.begin(); u!=e.vchild.end(); ++u) {
			fprintf(context.outfd,"{\"p\":" GENERIC_ARG_PID_FMT ",\"f\":%ld}",(*u).first,(*u).second);
			if (u!=e.vchild.end()-1) fprintf(context.outfd,",");
		}
		fprintf(context.outfd,"],\"r\":{\"p\":" GENERIC_ARG_PID_FMT ",\"x\":%u},\"i\":[",e.parent.first,e.parent.second);
		std::pair<upid_t,unsigned> pipe_map_key(e.pid,e.procidx);
		if (pipe_map.find(pipe_map_key)!=pipe_map.end()) {
			std::set<std::pair<upid_t,unsigned>>& pmv = pipe_map[pipe_map_key];
			for (auto u=pmv.begin(); u!=pmv.end(); ++u) {
				if (u!=pmv.begin()) fprintf(context.outfd,",");
				fprintf(context.outfd,"{\"p\":" GENERIC_ARG_PID_FMT ",\"x\":%u}",(*u).first,(*u).second);
			}
		}
		fprintf(context.outfd,"],\"o\":[");
		auto chi = rwi;
		unsigned long chsize = 0;
		for (; rwi!=e.rwmap.end(); ++rwi) {
			if (rwi!=chi) fprintf(context.outfd,",");
			if ((*rwi).first.second.length()<=0) {
				fprintf(context.outfd,"{\"p\":\"%s\",\"m\":%u,\"s\":%ld}",
						(*rwi).first.first.c_str(),(*rwi).second.first,(*rwi).second.second);
				chsize+=15+strlen((*rwi).first.first.c_str());
			}
			else {
				fprintf(context.outfd,"{\"p\":\"%s\",\"o\":\"%s\",\"m\":%u,\"s\":%ld}",
						(*rwi).first.first.c_str(),(*rwi).first.second.c_str(),(*rwi).second.first,(*rwi).second.second);
				chsize+=22+strlen((*rwi).first.first.c_str())+strlen((*rwi).first.second.c_str());
			}
#if SPLIT_EXEC_ENTRY_AT_SIZE>0
			if (chsize>SPLIT_EXEC_ENTRY_AT_SIZE) {
#else
			if (0) {
#endif
				/* We've surpassed a limit of 'SPLIT_EXEC_ENTRY_AT_SIZE'[B] for read/write entries (Mongo complains whenever single document
				 *  size is larger than 16MB, let's stay on the safe side here). Print another entry with the same
				 *  values except the read/write array which will have continuation of file entries */
				++rwi;
				if (rwi!=e.rwmap.end()) {
					fprintf(context.outfd,"]},\n");
					goto print_entry;
				}
				else break;
			}
		} /* for() */
		fprintf(context.outfd,"]}");
		if (interrupt) break;
	} /* for(i) */
	if (!interrupt) printf("\r100%%\n"); else printf("\n");
	if (interrupt) exit(2);
    fprintf(context.outfd,"]");
	fclose(context.outfd);

	return 0;
}

int main(int argc, char** argv) {

	return parser_main(argc,argv);
}
