extern "C" {
#include "pyftdb.h"
#include "utils.h"
}
#include <set>
#include <utility>
#include <map>
#include <vector>
#include <algorithm>
#include <string>

/*
 Some precomputed maps
refmap: 		type_id -> type entry
hrefmap:		type_hash -> type entry
frefmap:		func_id -> func_entry
fhrefmap:		func_hash -> func_entry
fnrefmap:		func_name -> [func_entry, func_entry...]
grefmap:		global_id -> global_entry
ghrefmap:		global_hash -> global_entry
gnrefmap:		global_name -> global_entry
fdrefmap:		fdecl_id -> func_decl_entry
fdhrefmap:		fdeclhash -> func_decl_entry
*/
int ftdb_maps(struct ftdb* ftdb, int show_stats) {

	for (unsigned long i=0; i<ftdb->types_count; ++i) {
		struct ftdb_type_entry* type_entry = &ftdb->types[i];
		ulong_entryMap_insert(&ftdb->refmap,type_entry->id,type_entry);
		stringRef_entryMap_insert(&ftdb->hrefmap, type_entry->hash, type_entry);
	}

	printf("refmap keys: %zu\n",ulong_entryMap_count(&ftdb->refmap));
	printf("hrefmap keys: %zu\n",stringRef_entryMap_count(&ftdb->hrefmap));

	std::map<std::string,std::vector<struct ftdb_func_entry*>> fnrefmap;
	for (unsigned long i=0; i<ftdb->funcs_count; ++i) {
		struct ftdb_func_entry* func_entry = &ftdb->funcs[i];
		ulong_entryMap_insert(&ftdb->frefmap,func_entry->id,func_entry);
		stringRef_entryMap_insert(&ftdb->fhrefmap, func_entry->hash, func_entry);
		fnrefmap[func_entry->name].push_back(func_entry);
	}

	printf("frefmap keys: %zu\n",ulong_entryMap_count(&ftdb->frefmap));
	printf("fhrefmap keys: %zu\n",stringRef_entryMap_count(&ftdb->fhrefmap));

	BUILD_STRINGREF_ENTRYLIST_MAP(ftdb,fnrefmap,fnrefmap);

	std::map<std::string,std::vector<struct ftdb_global_entry*>> gnrefmap;
	for (unsigned long i=0; i<ftdb->globals_count; ++i) {
		struct ftdb_global_entry* global_entry = &ftdb->globals[i];
		ulong_entryMap_insert(&ftdb->grefmap,global_entry->id,global_entry);
		stringRef_entryMap_insert(&ftdb->ghrefmap, global_entry->hash, global_entry);
		gnrefmap[global_entry->name].push_back(global_entry);
	}

	printf("grefmap keys: %zu\n",ulong_entryMap_count(&ftdb->grefmap));
	printf("ghrefmap keys: %zu\n",stringRef_entryMap_count(&ftdb->ghrefmap));

	BUILD_STRINGREF_ENTRYLIST_MAP(ftdb,gnrefmap,gnrefmap);

	for (unsigned long i=0; i<ftdb->funcdecls_count; ++i) {
		struct ftdb_funcdecl_entry* funcdecl_entry = &ftdb->funcdecls[i];
		ulong_entryMap_insert(&ftdb->fdrefmap,funcdecl_entry->id,funcdecl_entry);
		stringRef_entryMap_insert(&ftdb->fdhrefmap, funcdecl_entry->declhash, funcdecl_entry);
	}
	printf("fdrefmap keys: %zu\n",ulong_entryMap_count(&ftdb->fdrefmap));
	printf("fdhrefmap keys: %zu\n",stringRef_entryMap_count(&ftdb->fdhrefmap));

	return 1;
}
