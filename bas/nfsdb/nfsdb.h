#ifndef __NFSDB_H__
#define __NFSDB_H__

#include "maps.h"

/*
 * .nfsdb and .nfsdb.deps version tracking. Make sure to modify these values 
 *  after every change to libetrace that could affect backward compatibility.
 * 
 * NFSDB_MAGIC_NUMBER - distinguish .nfsdb database from other .img files
 * NFSDB_DEPS_MAGIC_NUMBER - distinguish .deps database from other .img files
 * LIBETRACE_VERSION - required etrace version to support file
 */
#define NFSDB_MAGIC_NUMBER			0x424453464e42494cULL	/* b'LIBNFSDB' */
#define NFSDB_DEPS_MAGIC_NUMBER		0x5350454442494cULL		/* b'LIBDEPS\0' */
#define LIBETRACE_VERSION			4ULL


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
	unsigned long size;
	unsigned long* original_path;
	unsigned long open_timestamp;
	unsigned long close_timestamp;
	const struct nfsdb_entry* opaque_entry;
};

struct pp_def {
	unsigned long name;
	unsigned long value;
};

struct nfsdb_entry_file_index {
	unsigned long nfsdb_index;
	unsigned long open_index;
	int __used;
};

struct compilation_info {
	unsigned long* compiled_list;
	struct nfsdb_entry_file_index* compiled_index;
	unsigned long compiled_count;
	unsigned long* include_paths;
	unsigned long include_paths_count;
	struct pp_def* pp_defs;
	unsigned long pp_defs_count;
	struct pp_def* pp_udefs;
	unsigned long pp_udefs_count;
	unsigned long* header_list;
	struct nfsdb_entry_file_index* header_index;
	unsigned long header_list_count;
	int compilation_type;
	int integrated_compilation;
	unsigned long* object_list;
	struct nfsdb_entry_file_index* object_index;
	unsigned long object_list_count;
};

struct nfsdb_entry {
	unsigned long nfsdb_index;
	struct eid eid;
	unsigned long stime;
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
	struct nfsdb_entry_file_index linked_index;
	int linked_type;
	int has_shared_argv;
	int return_code;
};

struct nfsdb_deps {
	/* nfsdb.deps.img header - DO NOT modify */
	unsigned long long db_magic;
	unsigned long long db_version;
	/* End of FTDB.img header */

	struct rb_root depmap;
	struct rb_root ddepmap;
	struct rb_root revdepmap;
	struct rb_root revddepmap;
};

struct nfsdb {
	/* nfsdb.img header - DO NOT modify */
	unsigned long long db_magic;
	unsigned long long db_version;
	/* End of FTDB.img header */

	struct nfsdb_entry* nfsdb;
	unsigned long nfsdb_count;
	const char* source_root;
	unsigned long source_root_size;
	const char* dbversion;
	const char** string_table;
	uint32_t* string_size_table;
	unsigned long string_table_size;
	unsigned long string_count;
	const char** pcp_pattern_list;
	unsigned long pcp_pattern_list_size;
	unsigned long* shared_argv_list;
	unsigned long shared_argv_list_size;
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
