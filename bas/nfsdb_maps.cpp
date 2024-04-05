extern "C" {
#include "pyetrace.h"
#include "utils.h"
}
#include <set>
#include <utility>
#include <map>
#include <vector>
#include <algorithm>
#include <string>
#include <list>

struct eid_cmp {
    bool operator() (const struct eid& a, const struct eid& b) const {
        if (a.pid<b.pid) return true;
    	if (a.pid>b.pid) return false;
        return a.exeidx < b.exeidx;
    }
};

unsigned long nfsdb_has_unique_keys(const struct nfsdb* nfsdb) {

	std::set<struct eid, eid_cmp> kset;
	for (unsigned long u=0; u<nfsdb->nfsdb_count; ++u) {
		if (kset.find(nfsdb->nfsdb[u].eid)!=kset.end()) {
			return u;
		}
		kset.insert(nfsdb->nfsdb[u].eid);
	}

	return 0;
}

#define BUILD_NFSDB_ENTRY_MAP(__name,__member)	\
	do {	\
		for (decltype(__name)::iterator i=__name.begin(); i!=__name.end(); ++i) {	\
			struct nfsdb_entry** entry_list = (struct nfsdb_entry**)malloc((*i).second.size()*sizeof(struct nfsdb_entry*));	\
			size_t u=0;	\
			for (decltype((*i).second)::iterator j=(*i).second.begin(); j!=(*i).second.end(); ++j,++u) {	\
				entry_list[u] = (*j);	\
			}	\
			nfsdb_entryMap_insert(&nfsdb->__member,(*i).first,entry_list,(*i).second.size());	\
		}	\
		if (show_stats) {	\
			printf(#__name " keys: %zu:%zu\n",__name.size(),nfsdb_entryMap_count(&nfsdb->__member));	\
			size_t __name##EntryCount = 0;	\
			for (decltype(__name)::iterator i=__name.begin(); i!=__name.end(); ++i) {	\
				__name##EntryCount+=(*i).second.size();	\
			}	\
			printf(#__name " entry count: %zu:%zu\n",__name##EntryCount,nfsdb_entryMap_entry_count(&nfsdb->__member));	\
		}	\
	} while(0)

#define BUILD_ULONG_MAP(__name,__member)	\
	do {	\
		for (decltype(__name)::iterator i=__name.begin(); i!=__name.end(); ++i) {	\
			unsigned long* value_list = (unsigned long*)malloc((*i).second.size()*sizeof(unsigned long));	\
			size_t u=0;	\
			for (decltype((*i).second)::iterator j=(*i).second.begin(); j!=(*i).second.end(); ++j,++u) {	\
				value_list[u] = (*j);	\
			}	\
			ulongMap_insert(&nfsdb->__member,(*i).first,value_list,(*i).second.size(),(*i).second.size());	\
		}	\
		if (show_stats) {	\
			printf(#__name " keys: %zu:%zu\n",__name.size(),ulongMap_count(&nfsdb->__member));	\
			size_t __name##EntryCount = 0;	\
			for (decltype(__name)::iterator i=__name.begin(); i!=__name.end(); ++i) {	\
				__name##EntryCount+=(*i).second.size();	\
			}	\
			printf(#__name " entry count: %zu:%zu\n",__name##EntryCount,ulongMap_entry_count(&nfsdb->__member));	\
		}	\
	}	\
	while(0)

#define BUILD_STRINGREF_ENTRYLIST_MAP(__ob,__name,__member)	\
	do {	\
		for (decltype(__name)::iterator i=__name.begin(); i!=__name.end(); ++i) {	\
			void** entry_list = (void**)malloc((*i).second.size()*sizeof(void*));	\
			size_t u=0;	\
			for (decltype((*i).second)::iterator j=(*i).second.begin(); j!=(*i).second.end(); ++j,++u) {	\
				entry_list[u] = (void*)(*j);	\
			}	\
			stringRef_entryListMap_insert(&__ob->__member,strdup((*i).first.c_str()),entry_list,(*i).second.size());	\
		}	\
		if (show_stats) {	\
			printf(#__name " keys: %zu:%zu\n",__name.size(),stringRef_entryListMap_count(&__ob->__member));	\
			size_t __name##EntryCount = 0;	\
			for (decltype(__name)::iterator i=__name.begin(); i!=__name.end(); ++i) {	\
				__name##EntryCount+=(*i).second.size();	\
			}	\
			printf(#__name " entry count: %zu:%zu\n",__name##EntryCount,stringRef_entryListMap_entry_count(&__ob->__member));	\
		}	\
	} while(0)

typedef std::vector<struct nfsdb_entry*> vexecs_t;
typedef std::list<struct nfsdb_entry*> lexecs_t;
typedef std::map<unsigned long,std::vector<unsigned long>> forkMap_t;

/*
 * processMap:
 *  Maps process <pid> with all nfsdb entries for this specific <pid>
 *  (several nfsdb entries might share the same <pid> as they comes from different executions of the same process)
 * {
 *   <pid> : [ <exec_entry>, ... ]
 *   (...)
 * }
 *
 * bexeMap:
 *   Maps binary path with all nfsdb entries which executed given binary
 * {
 *   <binary_path> : [ <exec_entry>, ... ]
 * }
 *
 * forkMap:
 *   Maps a given process to all the children processes executed in all executions of this process
 * {
 *   <pid> : [ <child_pid>, ... ]
 * }
 *
 * revforkMap:
 *   Maps a given process to its parent process
 * {
 *   <pid> : <parent_pid>
 * }
 *
 * pipeMap:
 *   Maps a given process to the unique list of other processes which could've possibly written to this process through pipe
 * {
 *   <pid> : { <other_pid>, ... }
 * }
 *
 * wrMap:
 *   Maps a given process to the unique list of file paths opened for writing in all executions of this process
 * {
 *   <pid> : { <filepath>, ... }
 * }
 *
 * revWrMap:
 *   Maps a file path to the unique list of processes that opened this file path for writing
 * {
 *   <filepath> : { <pid>, ... }
 * }
 *
  * rdMap:
 *   Maps a given process to the unique list of file paths opened for reading in all executions of this process
 * {
 *   <pid> : { <filepath>, ... }
 * }
 *
 * fileMap:
 *  Maps a unique file path to a list of open file handles that used this path (i.e. nfsdb entry and openfile index within this entry)
 *   per open mode (RD,WR,RW) + single global access set (either RD, WR or RW)
 *  For executable paths we also set the executable bit
 *
 * linkedMap:
 *  Maps a unique linked file path to a nfsdb entry that created this linked file path
 */

int nfsdb_maps(struct nfsdb* nfsdb, int show_stats) {

	std::map<unsigned long,vexecs_t> processMap;
	std::map<unsigned long,lexecs_t> bexeMap;
	forkMap_t forkMap;
	typedef std::pair<unsigned long,unsigned long> openfileHandle;
	typedef std::set<openfileHandle> openfileHandleSet;
	std::map<unsigned long,std::set<unsigned long>> pipeMap;
	std::map<unsigned long,std::set<unsigned long>> wrMap;
	std::map<unsigned long,std::set<unsigned long>> rdMap;
	std::map<unsigned long,std::tuple<openfileHandleSet,openfileHandleSet,openfileHandleSet,openfileHandleSet,unsigned long>> fileMap;
	std::map<unsigned long,struct nfsdb_entry*> linkedMap;
	for (unsigned long u=0; u<nfsdb->nfsdb_count; ++u) {

		struct nfsdb_entry* entry = &nfsdb->nfsdb[u];

		unsigned long pid = entry->eid.pid;
		processMap[pid].push_back(entry);

		if ((entry->bpath!=LIBETRACE_EMPTY_STRING_HANDLE)&&(entry->argv_count>0)) {
			/* If the entry is a compiler or linker move the first such entry at the beginning of the 'bexeMap'
			   (it can ease the burden of checking whether a given path is a compiler or linker) */
			if ((entry->compilation_info)||(entry->linked_file)) {
				bexeMap[entry->bpath].push_front(entry);
			}
			else {
				bexeMap[entry->bpath].push_back(entry);
			}
		}

		for (unsigned long i=0; i<entry->child_ids_count; ++i) {
			forkMap[pid].push_back(entry->child_ids[i].pid);
		}

		for (unsigned long i=0; i<entry->pipe_eids_count; ++i) {
			pipeMap[entry->pipe_eids[i].pid].insert(pid);
		}

		for (unsigned long i=0; i<entry->open_files_count; ++i) {
			if ((entry->open_files[i].mode&3)>=1) {
				wrMap[pid].insert(entry->open_files[i].path);
				std::get<1>(fileMap[entry->open_files[i].path]).insert(openfileHandle(u,i));
			}
			if ((entry->open_files[i].mode&3)!=1) {
				rdMap[pid].insert(entry->open_files[i].path);
				std::get<0>(fileMap[entry->open_files[i].path]).insert(openfileHandle(u,i));
			}
		}

		if (entry->linked_file) {
			linkedMap[*(entry->linked_file)] = entry;
		}
	}

	/* Populate the return value of the last execution in a process to all other executions */
	for (std::map<unsigned long,vexecs_t>::iterator i=processMap.begin(); i!=processMap.end(); ++i) {
		vexecs_t& execs = (*i).second;
		struct nfsdb_entry* last_entry = execs.back();
		for (vexecs_t::iterator j=execs.begin(); j!=execs.end(); ++j) {
			(*j)->return_code = last_entry->return_code;
		}
	}

	std::map<unsigned long,unsigned long> revforkMap;
	for (forkMap_t::iterator i=forkMap.begin(); i!=forkMap.end(); ++i) {
		auto vc = (*i).second;
		for (decltype(vc)::iterator j=vc.begin(); j!=vc.end(); ++j) {
			if (revforkMap.find((*j))!=revforkMap.end()) {
				printf("ERROR: multiple parents for process %lu\n",*j);
				return 0;
			}
			else {
				revforkMap.insert(std::pair<unsigned long,unsigned long>(*j,(*i).first));
			}
		}
	}

	BUILD_NFSDB_ENTRY_MAP(processMap,procmap);
	BUILD_NFSDB_ENTRY_MAP(bexeMap,bmap);
	BUILD_ULONG_MAP(forkMap,forkmap);

	/* Check if the executed binary path exists after the build */
	struct rb_node * p = rb_first(&nfsdb->bmap);
	while(p) {
		struct nfsdb_entryMap_node* data = (struct nfsdb_entryMap_node*)p;
		const char* binary_path = nfsdb->string_table[data->key];
		if (access(binary_path, F_OK) == 0) {
			/* exists */
			data->custom_data = 1;
			/* is this a link? */
			struct stat stat_buf;
			if (!lstat(binary_path,&stat_buf)) {
				if (S_ISLNK(stat_buf.st_mode)) data->custom_data++;
			}
		}
		p = rb_next(p);
	}

	for (decltype(revforkMap)::iterator i=revforkMap.begin(); i!=revforkMap.end(); ++i) {
		unsigned long* value_list = (unsigned long*)malloc(sizeof(unsigned long));
		value_list[0] = (*i).second;
		ulongMap_insert(&nfsdb->revforkmap,(*i).first,value_list,1,1);
	}
	if (show_stats) {
		printf("revforkMap keys: %zu:%zu\n",revforkMap.size(),ulongMap_count(&nfsdb->revforkmap));
	}

	BUILD_ULONG_MAP(pipeMap,pipemap);
	BUILD_ULONG_MAP(wrMap,wrmap);
	BUILD_ULONG_MAP(rdMap,rdmap);

	for (unsigned long i=0; i<nfsdb->string_count; ++i) {
		const char* s = nfsdb->string_table[i];
		stringRefMap_insert(&nfsdb->revstringmap, s, i);
	}

	for (decltype(fileMap)::iterator i=fileMap.begin(); i!=fileMap.end(); ++i) {
		openfileHandleSet& rdSet = std::get<0>((*i).second);
		openfileHandleSet& wrSet = std::get<1>((*i).second);
		openfileHandleSet& rwSet = std::get<2>((*i).second);
		openfileHandleSet& gaSet = std::get<3>((*i).second);

		/* Fill the global access set that will be used for filtering of referenced files */
		if ((rdSet.size()>0)&&(wrSet.size()>0)) {
			gaSet.insert(rdSet.begin(),rdSet.end());
			gaSet.insert(wrSet.begin(),wrSet.end());
			std::get<4>((*i).second) = ACCESS_RW;
		}
		else if (rdSet.size()>0) {
			gaSet.insert(rdSet.begin(),rdSet.end());
			std::get<4>((*i).second) = ACCESS_READ;
		}
		else {
			gaSet.insert(wrSet.begin(),wrSet.end());
			std::get<4>((*i).second) = ACCESS_WRITE;
		}

		std::set_intersection(rdSet.begin(), rdSet.end(),
							  wrSet.begin(), wrSet.end(),
							  std::inserter(rwSet,rwSet.begin()));
		for (openfileHandleSet::iterator u=rwSet.begin(); u!=rwSet.end(); ++u) {
			rdSet.erase(*u);
			wrSet.erase(*u);
		}

		/* Flush file map entries to cache structures */
		struct nfsdb_fileMap_node* node = fileMap_insert_key(&nfsdb->filemap, (*i).first);
		node->rd_entry_list = (struct nfsdb_entry**)malloc(rdSet.size()*sizeof(struct nfsdb_entry*));
		node->rd_entry_index = (unsigned long*)malloc(rdSet.size()*sizeof(unsigned long));
		node->rd_entry_count = rdSet.size();
		size_t u=0;
		for (openfileHandleSet::iterator j=rdSet.begin(); j!=rdSet.end(); ++j,++u) {
			node->rd_entry_list[u] = &nfsdb->nfsdb[(*j).first];
			node->rd_entry_index[u] = (*j).second;
		}
		node->wr_entry_list = (struct nfsdb_entry**)malloc(wrSet.size()*sizeof(struct nfsdb_entry*));
		node->wr_entry_index = (unsigned long*)malloc(wrSet.size()*sizeof(unsigned long));
		node->wr_entry_count = wrSet.size();
		u=0;
		for (openfileHandleSet::iterator j=wrSet.begin(); j!=wrSet.end(); ++j,++u) {
			node->wr_entry_list[u] = &nfsdb->nfsdb[(*j).first];
			node->wr_entry_index[u] = (*j).second;
		}
		node->rw_entry_list = (struct nfsdb_entry**)malloc(rwSet.size()*sizeof(struct nfsdb_entry*));
		node->rw_entry_index = (unsigned long*)malloc(rwSet.size()*sizeof(unsigned long));
		node->rw_entry_count = rwSet.size();
		u=0;
		for (openfileHandleSet::iterator j=rwSet.begin(); j!=rwSet.end(); ++j,++u) {
			node->rw_entry_list[u] = &nfsdb->nfsdb[(*j).first];
			node->rw_entry_index[u] = (*j).second;
		}
		node->ga_entry_list = (struct nfsdb_entry**)malloc(gaSet.size()*sizeof(struct nfsdb_entry*));
		node->ga_entry_index = (unsigned long*)malloc(gaSet.size()*sizeof(unsigned long));
		node->ga_entry_count = gaSet.size();
		u=0;
		for (openfileHandleSet::iterator j=gaSet.begin(); j!=gaSet.end(); ++j,++u) {
			node->ga_entry_list[u] = &nfsdb->nfsdb[(*j).first];
			node->ga_entry_index[u] = (*j).second;
		}
		node->global_access = std::get<4>((*i).second);
		node->access_type = FILE_ACCESS_TYPE_OPEN;
	}
	/* Now add the paths of executed binaries to the fileMap and mark the entries as executables accordingly */
	p = rb_first(&nfsdb->bmap);
	while(p) {
		struct nfsdb_entryMap_node* data = (struct nfsdb_entryMap_node*)p;
		struct nfsdb_fileMap_node* node = fileMap_search(&nfsdb->filemap,data->key);
		if (!node) {
			node = fileMap_insert_key(&nfsdb->filemap, data->key);
			node->access_type = FILE_ACCESS_TYPE_EXEC;
		}
		else {
			node->access_type = FILE_ACCESS_TYPE_OPENEXEC;
		}
		p = rb_next(p);
	}

	if (show_stats) {
		printf("fileMap" " keys: %zu:%zu\n",fileMap.size(),fileMap_count(&nfsdb->filemap));
		size_t fileMapRdEntryCount = 0;
		size_t fileMapWrEntryCount = 0;
		size_t fileMapRwEntryCount = 0;
		for (decltype(fileMap)::iterator i=fileMap.begin(); i!=fileMap.end(); ++i) {
			fileMapRdEntryCount+=std::get<0>((*i).second).size();
			fileMapWrEntryCount+=std::get<1>((*i).second).size();
			fileMapRwEntryCount+=std::get<2>((*i).second).size();
		}
		printf("fileMap" " rd entry count: %zu:%zu\n",fileMapRdEntryCount,fileMap_rd_entry_count(&nfsdb->filemap));
		printf("fileMap" " wr entry count: %zu:%zu\n",fileMapWrEntryCount,fileMap_wr_entry_count(&nfsdb->filemap));
		printf("fileMap" " rw entry count: %zu:%zu\n",fileMapRwEntryCount,fileMap_rw_entry_count(&nfsdb->filemap));
	}

	for (decltype(linkedMap)::iterator i=linkedMap.begin(); i!=linkedMap.end(); ++i) {
		struct nfsdb_entry** entry_list = (struct nfsdb_entry**)malloc(sizeof(struct nfsdb_entry*));
		entry_list[0] = (*i).second;
		nfsdb_entryMap_insert(&nfsdb->linkedmap,(*i).first,entry_list,1);
	}
	if (show_stats) {
		printf("linkedMap entry count: %zu\n",nfsdb_entryMap_count(&nfsdb->linkedmap));
	}

	return 1;
}
