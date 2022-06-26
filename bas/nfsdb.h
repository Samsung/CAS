#ifndef __NFSDB_H__
#define __NFSDB_H__

#include "maps.h"

struct eid {
	unsigned long pid;
	unsigned long exeidx;
};

struct cid {
	unsigned long pid;
	unsigned long flags;
};

struct openfile {
	unsigned long path;
	unsigned long mode;
	unsigned long* original_path;
};

struct pp_def {
	unsigned long name;
	unsigned long value;
};

struct compilation_info {
	unsigned long* compiled_list;
	unsigned long compiled_count;
	unsigned long* include_paths;
	unsigned long include_paths_count;
	struct pp_def* pp_defs;
	unsigned long pp_defs_count;
	struct pp_def* pp_udefs;
	unsigned long pp_udefs_count;
	unsigned long* header_list;
	unsigned long header_list_count;
	int compilation_type;
	unsigned long* object_list;
	unsigned long object_list_count;
};

struct nfsdb_entry {
	unsigned long nfsdb_index;
	struct eid eid;
	unsigned long etime;
	struct eid parent_eid;
	struct cid* child_ids;
	unsigned long child_ids_count;
	unsigned long binary;
	unsigned long cwd;
	unsigned long bpath;
	unsigned long* argv;
	unsigned long argv_count;
	struct openfile* open_files;
	unsigned long open_files_count;
	unsigned char* pcp;
	unsigned long pcp_count;
	unsigned long wrapper_pid;
	struct eid* pipe_eids;
	unsigned long pipe_eids_count;
	struct compilation_info* compilation_info;
	unsigned long* linked_file;
	int linked_type;
};

struct nfsdb_deps {
	struct rb_root depmap;
	struct rb_root ddepmap;
};

struct nfsdb {
	struct nfsdb_entry* nfsdb;
	unsigned long nfsdb_count;
	const char* source_root;
	const char* dbversion;
	const char** string_table;
	uint32_t* string_size_table;
	unsigned long string_table_size;
	unsigned long string_count;
	const char** pcp_pattern_list;
	unsigned long pcp_pattern_list_size;
	struct rb_root procmap;
	struct rb_root bmap;
	struct rb_root forkmap;
	struct rb_root revforkmap;
	struct rb_root pipemap;
	struct rb_root wrmap;
	struct rb_root revwrmap;
	struct rb_root rdmap;
	struct rb_root revstringmap;
	struct rb_root filemap;
	struct rb_root linkedmap;
};

#endif /* __NFSDB_H__ */
