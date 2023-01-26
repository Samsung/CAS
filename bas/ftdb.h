#ifndef __FTDB_H__
#define __FTDB_H__

#include "maps.h"

enum functionLinkage {
	LINKAGE_NONE,
	LINKAGE_INTERNAL,
	LINKAGE_EXTERNAL,
	LINKAGE_OTHER,
};

static enum functionLinkage get_functionLinkage(const char* s) {
	if (!strcmp(s,"internal")) {
		return LINKAGE_INTERNAL;
	}
	else if (!strcmp(s,"external")) {
		return LINKAGE_EXTERNAL;
	}
	else if ((!strcmp(s,"none"))||(strlen(s)==0)) {
		return LINKAGE_NONE;
	}
	return LINKAGE_OTHER;
}

static const char* set_functionLinkage(enum functionLinkage linkage) {
	if (linkage==LINKAGE_INTERNAL) {
		return "internal";
	}
	else if (linkage==LINKAGE_EXTERNAL) {
		return "external";
	}
	else if (linkage==LINKAGE_NONE) {
		return "none";
	}

	return "other";
}


enum globalDefType {
	DEFTYPE_A = 0,
	DEFTYPE_B = 1,
	DEFTYPE_C = 2,
};

static enum globalDefType get_globalDefType(int v) {
	return (enum globalDefType)v;
}

static int set_globalDefType(enum globalDefType type) {
	return (int)type;
}

enum exprType {
	EXPR_FUNCTION,
	EXPR_GLOBAL,
	EXPR_LOCAL,
	EXPR_LOCALPARM,
	EXPR_STRINGLITERAL,
	EXPR_CHARLITERAL,
	EXPR_INTEGERLITERAL,
	EXPR_FLOATLITERAL,
	EXPR_CALLREF,
	EXPR_REFCALLREF,
	EXPR_ADDRCALLREF,
	EXPR_ADDRESS,
	EXPR_INTEGER,
	EXPR_FLOATING,
	EXPR_STRING,
	EXPR_UNARY,
	EXPR_ARRAY,
	EXPR_MEMBER,
	EXPR_ASSIGN,
	EXPR_OFFSETOF,
	EXPR_INIT,
	EXPR_RETURN,
	EXPR_COND,
	EXPR_LOGIC,
	EXPR_UNDEF,
};

static enum exprType get_exprType(const char* s) {
	if (!strcmp(s,"function")) {
		return EXPR_FUNCTION;
	}
	else if (!strcmp(s,"global")) {
		return EXPR_GLOBAL;
	}
	else if (!strcmp(s,"local")) {
		return EXPR_LOCAL;
	}
	else if (!strcmp(s,"parm")) {
		return EXPR_LOCALPARM;
	}
	else if (!strcmp(s,"string_literal")) {
		return EXPR_STRINGLITERAL;
	}
	else if (!strcmp(s,"char_literal")) {
		return EXPR_CHARLITERAL;
	}
	else if (!strcmp(s,"integer_literal")) {
		return EXPR_INTEGERLITERAL;
	}
	else if (!strcmp(s,"float_literal")) {
		return EXPR_FLOATLITERAL;
	}
	else if (!strcmp(s,"callref")) {
		return EXPR_CALLREF;
	}
	else if (!strcmp(s,"refcallref")) {
		return EXPR_REFCALLREF;
	}
	else if (!strcmp(s,"addrcallref")) {
		return EXPR_ADDRCALLREF;
	}
	else if (!strcmp(s,"address")) {
		return EXPR_ADDRESS;
	}
	else if (!strcmp(s,"integer")) {
		return EXPR_INTEGER;
	}
	else if (!strcmp(s,"float")) {
		return EXPR_FLOATING;
	}
	else if (!strcmp(s,"string")) {
		return EXPR_STRING;
	}
	else if (!strcmp(s,"unary")) {
		return EXPR_UNARY;
	}
	else if (!strcmp(s,"array")) {
		return EXPR_ARRAY;
	}
	else if (!strcmp(s,"member")) {
		return EXPR_MEMBER;
	}
	else if (!strcmp(s,"assign")) {
		return EXPR_ASSIGN;
	}
	else if (!strcmp(s,"offsetof")) {
		return EXPR_OFFSETOF;
	}
	else if (!strcmp(s,"init")) {
		return EXPR_INIT;
	}
	else if (!strcmp(s,"return")) {
		return EXPR_RETURN;
	}
	else if (!strcmp(s,"cond")) {
		return EXPR_COND;
	}
	else if (!strcmp(s,"logic")) {
		return EXPR_LOGIC;
	}

	return EXPR_UNDEF;
}

static const char* set_exprType(enum exprType exprType) {

	if (exprType==EXPR_FUNCTION) {
		return "function";
	}
	else if (exprType==EXPR_GLOBAL) {
		return "global";
	}
	else if (exprType==EXPR_LOCAL) {
		return "local";
	}
	else if (exprType==EXPR_LOCALPARM) {
		return "parm";
	}
	else if (exprType==EXPR_STRINGLITERAL) {
		return "string_literal";
	}
	else if (exprType==EXPR_CHARLITERAL) {
		return "char_literal";
	}
	else if (exprType==EXPR_INTEGERLITERAL) {
		return "integer_literal";
	}
	else if (exprType==EXPR_FLOATLITERAL) {
		return "float_literal";
	}
	else if (exprType==EXPR_CALLREF) {
		return "callref";
	}
	else if (exprType==EXPR_REFCALLREF) {
		return "refcallref";
	}
	else if (exprType==EXPR_ADDRCALLREF) {
		return "addrcallref";
	}
	else if (exprType==EXPR_ADDRESS) {
		return "address";
	}
	else if (exprType==EXPR_INTEGER) {
		return "integer";
	}
	else if (exprType==EXPR_FLOATING) {
		return "float";
	}
	else if (exprType==EXPR_STRING) {
		return "string";
	}
	else if (exprType==EXPR_UNARY) {
		return "unary";
	}
	else if (exprType==EXPR_ARRAY) {
		return "array";
	}
	else if (exprType==EXPR_MEMBER) {
		return "member";
	}
	else if (exprType==EXPR_ASSIGN) {
		return "assign";
	}
	else if (exprType==EXPR_OFFSETOF) {
		return "offsetof";
	}
	else if (exprType==EXPR_INIT) {
		return "init";
	}
	else if (exprType==EXPR_RETURN) {
		return "return";
	}
	else if (exprType==EXPR_COND) {
		return "cond";
	}
	else if (exprType==EXPR_LOGIC) {
		return "logic";
	}

	return "undef";
}

enum TypeClass {
	TYPECLASS_EMPTYRECORD,
	TYPECLASS_BUILTIN,
	TYPECLASS_POINTER,
	TYPECLASS_MEMBERPOINTER,
	TYPECLASS_ATTRIBUTED,
	TYPECLASS_COMPLEX,
	TYPECLASS_VECTOR,
	TYPECLASS_EXTENDEDVECTOR,
	TYPECLASS_DEPENDENT_SIZED_ARRAY,
	TYPECLASS_PACKEXPANSION,
	TYPECLASS_UNRESOLVEDUSING,
	TYPECLASS_AUTO,
	TYPECLASS_TEMPLATETYPEPARM,
	TYPECLASS_DEPENDENTNAME,
	TYPECLASS_SUBSTTEMPLATETYPEPARM,
	TYPECLASS_RECORDSPECIALIZATION,
	TYPECLASS_RECORDTEMPLATE,
	TYPECLASS_RECORD,
	TYPECLASS_RECORDFORWARD,
	TYPECLASS_CONSTARRAY,
	TYPECLASS_INCOMPLETEARRAY,
	TYPECLASS_VARIABLEARRAY,
	TYPECLASS_TYPEDEF,
	TYPECLASS_ENUM,
	TYPECLASS_ENUMFORWARD,
	TYPECLASS_DECAYEDPOINTER,
	TYPECLASS_FUNCTION,
	TYPECLASS_UNDEF,
};

static enum TypeClass get_TypeClass(const char* s) {
	if (!strcmp(s,"empty_record")) {
		return TYPECLASS_EMPTYRECORD;
	}
	else if (!strcmp(s,"builtin")) {
		return TYPECLASS_BUILTIN;
	}
	else if (!strcmp(s,"pointer")) {
		return TYPECLASS_POINTER;
	}
	else if (!strcmp(s,"member_pointer")) {
		return TYPECLASS_MEMBERPOINTER;
	}
	else if (!strcmp(s,"attributed")) {
		return TYPECLASS_ATTRIBUTED;
	}
	else if (!strcmp(s,"complex")) {
		return TYPECLASS_COMPLEX;
	}
	else if (!strcmp(s,"vector")) {
		return TYPECLASS_VECTOR;
	}
	else if (!strcmp(s,"extended_vector")) {
		return TYPECLASS_EXTENDEDVECTOR;
	}
	else if (!strcmp(s,"dependent_sized_array")) {
		return TYPECLASS_DEPENDENT_SIZED_ARRAY;
	}
	else if (!strcmp(s,"pack_expansion")) {
		return TYPECLASS_PACKEXPANSION;
	}
	else if (!strcmp(s,"unresolved_using")) {
		return TYPECLASS_UNRESOLVEDUSING;
	}
	else if (!strcmp(s,"auto")) {
		return TYPECLASS_AUTO;
	}
	else if (!strcmp(s,"template_type_parm")) {
		return TYPECLASS_TEMPLATETYPEPARM;
	}
	else if (!strcmp(s,"dependent_name")) {
		return TYPECLASS_DEPENDENTNAME;
	}
	else if (!strcmp(s,"subst_template_type_parm")) {
		return TYPECLASS_SUBSTTEMPLATETYPEPARM;
	}
	else if (!strcmp(s,"record_specialization")) {
		return TYPECLASS_RECORDSPECIALIZATION;
	}
	else if (!strcmp(s,"record_template")) {
		return TYPECLASS_RECORDTEMPLATE;
	}
	else if (!strcmp(s,"record")) {
		return TYPECLASS_RECORD;
	}
	else if (!strcmp(s,"record_forward")) {
		return TYPECLASS_RECORDFORWARD;
	}
	else if (!strcmp(s,"const_array")) {
		return TYPECLASS_CONSTARRAY;
	}
	else if (!strcmp(s,"incomplete_array")) {
		return TYPECLASS_INCOMPLETEARRAY;
	}
	else if (!strcmp(s,"variable_array")) {
		return TYPECLASS_VARIABLEARRAY;
	}
	else if (!strcmp(s,"typedef")) {
		return TYPECLASS_TYPEDEF;
	}
	else if (!strcmp(s,"enum")) {
		return TYPECLASS_ENUM;
	}
	else if (!strcmp(s,"enum_forward")) {
		return TYPECLASS_ENUMFORWARD;
	}
	else if (!strcmp(s,"decayed_pointer")) {
		return TYPECLASS_DECAYEDPOINTER;
	}
	else if (!strcmp(s,"function")) {
		return TYPECLASS_FUNCTION;
	}

	return TYPECLASS_UNDEF;
}

struct taint_element {
	unsigned long taint_level;
	unsigned long varid;
};

struct taint_data {
	struct taint_element* taint_list;
	unsigned long taint_list_count;
};

struct callref_data {
	enum exprType type;
	unsigned long pos;
	unsigned long* id;
	int64_t* integer_literal;
	int* character_literal;
	double* floating_literal;
	const char* string_literal;
};

struct callref_info {
	struct callref_data* callarg;
	unsigned long callarg_count;
};

struct refcall {
	unsigned long fid;
	unsigned long cid;
	unsigned long field_index;
	int ismembercall;
};

struct call_info {
	const char* start;
	const char* end;
	unsigned long ord;
	const char* expr;
	const char* loc;
	unsigned long* args;
	unsigned long args_count;
};

struct case_data {
	long expr_value;
	const char* enum_code;
	const char* macro_code;
	const char* raw_code;
};

struct case_info {
	struct case_data lhs;
	struct case_data* rhs;	/* optional */
};

struct switch_info {
	const char* condition;
	struct case_info* cases;
	unsigned long cases_count;
};

struct csitem {
	int64_t pid;
	unsigned long id;
	const char* cf;
};

struct local_info {
	unsigned long id;
	const char* name;
	int isparm;
	unsigned long type;
	int isstatic;
	int isused;
	int64_t* csid;	/* optional */
	const char* location;
};

struct offsetref_info {
	enum exprType kind;
	const char* id_string;
	double id_float;
	int64_t id_integer;
	unsigned long* mi;						/* optional */
	unsigned long* di;						/* optional */
	unsigned long* cast;					/* optional */
};

struct deref_info {
	enum exprType kind;
	int64_t* offset;						/* optional */
	unsigned long* basecnt;					/* optional */
	struct offsetref_info* offsetrefs;
	unsigned long offsetrefs_count;
	const char* expr;
	int64_t csid;
	unsigned long* ord;
	unsigned long ord_count;
	int64_t* member;						/* optional */
	unsigned long member_count;
	unsigned long* type;					/* optional */
	unsigned long type_count;
	unsigned long* access;					/* optional */
	unsigned long access_count;
	int64_t* shift;							/* optional */
	unsigned long shift_count;
	int64_t* mcall;							/* optional */
	unsigned long mcall_count;
};

struct ifref_info {
	enum exprType type;
	unsigned long* id;
	int64_t* integer_literal;
	int* character_literal;
	double* floating_literal;
	const char* string_literal;
};

struct if_info {
	int64_t csid;
	struct ifref_info* refs;
	unsigned long refs_count;
};

struct asm_info {
	int64_t csid;
	const char* str;
};

struct globalref_info {
	const char* start;
	const char* end;
};

struct globalref_data {
	struct globalref_info* refs;
	unsigned long refs_count;
};

struct ftdb_func_entry {
	unsigned long __index;
	const char* name;
	const char* __namespace;			/* optional */
	unsigned long id;
	unsigned long fid;
	unsigned long* fids;
	unsigned long fids_count;
	unsigned long* mids;
	unsigned long mids_count;			/* optional */
	unsigned long nargs;
	int isvariadic;
	const char* firstNonDeclStmt;
	int* isinline;						/* optional */
	int* istemplate; 					/* optional */
	const char* classOuterFn; 			/* optional */
	enum functionLinkage linkage;
	int* ismember; 						/* optional */
	const char* __class; 				/* optional */
	unsigned long* classid; 			/* optional */
	const char** attributes;
	unsigned long attributes_count;
	const char* hash;
	const char* cshash;
	const char* template_parameters;	/* optional */
	const char* body;
	const char* unpreprocessed_body;
	const char* declbody;
	const char* signature;
	const char* declhash;
	const char* location;
	const char* start_loc;
	const char* end_loc;
	unsigned long refcount;
	int64_t* integer_literals;
	unsigned long integer_literals_count;
	unsigned int* character_literals;
	unsigned long character_literals_count;
	double* floating_literals;
	unsigned long floating_literals_count;
	const char** string_literals;
	unsigned long string_literals_count;
	unsigned long declcount;
	struct taint_data* taint;
	unsigned long taint_count;
	unsigned long* calls;
	struct call_info* call_info;
	struct callref_info* callrefs;
	unsigned long calls_count;
	struct refcall* refcalls;
	struct call_info* refcall_info;
	unsigned long refcalls_count;
	struct callref_info* refcallrefs;
	struct switch_info* switches;
	unsigned long switches_count;
	struct csitem* csmap;
	unsigned long csmap_count;
	struct local_info* locals;
	unsigned long locals_count;
	struct deref_info* derefs;
	unsigned long derefs_count;
	struct if_info* ifs;
	unsigned long ifs_count;
	struct asm_info* asms;
	unsigned long asms_count;
	unsigned long* globalrefs;
	struct globalref_data* globalrefInfo;
	unsigned long globalrefs_count;
	unsigned long* funrefs;
	unsigned long funrefs_count;
	unsigned long* refs;
	unsigned long refs_count;
	unsigned long* decls;
	unsigned long decls_count;
	unsigned long* types;
	unsigned long types_count;
};

struct ftdb_funcdecl_entry {
	unsigned long __index;
	const char* name;
	const char* __namespace;			/* optional */
	unsigned long id;
	unsigned long fid;
	unsigned long nargs;
	int isvariadic;
	int* istemplate; 					/* optional */
	enum functionLinkage linkage;
	int* ismember; 						/* optional */
	const char* __class; 				/* optional */
	unsigned long* classid; 			/* optional */
	const char* template_parameters;	/* optional */
	const char* decl;
	const char* signature;
	const char* declhash;
	const char* location;
	unsigned long refcount;
	unsigned long* types;
	unsigned long types_count;
};

struct ftdb_unresolvedfunc_entry {
	const char* name;
	unsigned long id;
};

struct ftdb_global_entry {
	unsigned long __index;
	const char* name;
	const char* hash;
	unsigned long id;
	const char* def;
	unsigned long* globalrefs;
	unsigned long globalrefs_count;
	unsigned long* refs;
	unsigned long refs_count;
	unsigned long* funrefs;
	unsigned long funrefs_count;
	unsigned long* decls;
	unsigned long decls_count;
	unsigned long fid;
	unsigned long* mids;
	unsigned long mids_count;
	unsigned long type;
	enum functionLinkage linkage;
	const char* location;
	enum globalDefType deftype;
	int hasinit;
	const char* init;
	int64_t* integer_literals;
	unsigned long integer_literals_count;
	unsigned int* character_literals;
	unsigned long character_literals_count;
	double* floating_literals;
	unsigned long floating_literals_count;
	const char** string_literals;
	unsigned long string_literals_count;
};

struct bitfield {
	unsigned long index;
	unsigned long bitwidth;
};

struct ftdb_type_entry {
	unsigned long __index;
	unsigned long id;
	unsigned long fid;
	const char* hash;
	const char* class_name;
	enum TypeClass __class;
	const char* qualifiers;
	unsigned long size;
	const char* str;
	unsigned long refcount;
	unsigned long* refs;
	unsigned long refs_count;
	long* usedrefs;							/* optional */
	unsigned long usedrefs_count;
	unsigned long* decls;					/* optional */
	unsigned long decls_count;
	const char* def;						/* optional */
	unsigned long* memberoffsets;			/* optional */
	unsigned long memberoffsets_count;
	unsigned long* attrrefs;				/* optional */
	unsigned long attrrefs_count;
	unsigned long* attrnum;					/* optional */
	const char* name;						/* optional */
	int* isdependent;						/* optional */
	unsigned long* globalrefs;				/* optional */
	unsigned long globalrefs_count;
	int64_t* enumvalues;					/* optional */
	unsigned long enumvalues_count;
	const char* outerfn;					/* optional */
	unsigned long* outerfnid;				/* optional */
	int* isimplicit;						/* optional */
	int* isunion;							/* optional */
	const char** refnames;					/* optional */
	unsigned long refnames_count;
	const char* location;					/* optional */
	int* isvariadic;						/* optional */
	unsigned long* funrefs;					/* optional */
	unsigned long funrefs_count;
	const char** useddef;					/* optional */
	unsigned long useddef_count;
	const char* defhead;					/* optional */
	const char** identifiers;				/* optional */
	unsigned long identifiers_count;
	unsigned long* methods;					/* optional */
	unsigned long methods_count;
	struct bitfield* bitfields;				/* optional */
	unsigned long bitfields_count;
};

struct fops_member_info {
	unsigned long index;
	unsigned long fnid;
};

struct ftdb_fops_var_entry {
	unsigned long __index;
	const char* type;
	const char* name;
	struct fops_member_info* members;
	unsigned long members_count;
	const char* location;
};

struct ftdb_fops {
	struct ftdb_fops_var_entry* vars;
	unsigned long vars_count;
	unsigned long member_count;
};

struct matrix_data {
	unsigned long* data;
	unsigned long data_count;
	unsigned long* row_ind;
	unsigned long row_ind_count;
	unsigned long* col_ind;
	unsigned long col_ind_count;
	unsigned long matrix_size;
};

struct func_map_entry {
	unsigned long id;
	unsigned long* fids;
	unsigned long fids_count;
};

struct size_dep_item {
	unsigned long id;
	unsigned long add;
};

struct init_data_item {
	unsigned long* id;
	const char** name;
	unsigned long name_count;
	unsigned long* size;
	struct size_dep_item* size_dep;
	const char* nullterminated;
	const char* tagged;
	const char* fuzz;
	const char* pointer;
        long* min_value;
        long* max_value;
        long* value;
        const char* user_name;
};

struct init_data_entry {
	const char* name;
	unsigned long* order;
	unsigned long order_count;
	const char* interface;
	struct init_data_item* items;
	unsigned long items_count;
};

struct known_data_entry {
	const char* version;
	unsigned long* func_ids;
	unsigned long func_ids_count;
	unsigned long* builtin_ids;
	unsigned long builtin_ids_count;
	unsigned long* asm_ids;
	unsigned long asm_ids_count;
	const char** lib_funcs;
	unsigned long lib_funcs_count;
	unsigned long* lib_funcs_ids;
	unsigned long lib_funcs_ids_count;
	unsigned long* always_inc_funcs_ids;
	unsigned long always_inc_funcs_ids_count;
};

struct BAS_item {
	const char* loc;
	const char** entries;
	unsigned long entries_count;
};

struct macro_occurence {
	const char* loc;
	const char* expanded;
	unsigned long fid;
};

struct macroinfo_item {
	const char* name;
	struct macro_occurence* occurences;
	unsigned long occurences_count;
};

struct ftdb {
	struct ftdb_func_entry* funcs;
	unsigned long funcs_count;
	struct ftdb_funcdecl_entry* funcdecls;
	unsigned long funcdecls_count;
	struct ftdb_unresolvedfunc_entry* unresolvedfuncs;
	unsigned long unresolvedfuncs_count;
	struct ftdb_global_entry* globals;
	unsigned long globals_count;
	struct ftdb_type_entry* types;
	unsigned long types_count;
	struct ftdb_fops fops;
	struct rb_root sourcemap;
	const char** sourceindex_table;
	unsigned long sourceindex_table_count;
	struct rb_root modulemap;
	const char** moduleindex_table;
	unsigned long moduleindex_table_count;
	struct macroinfo_item* macroinfo;
	unsigned long macroinfo_count;
	struct rb_root macromap;
	const char* version;
	const char* module;
	const char* directory;
	const char* release;
	struct rb_root refmap;
	struct rb_root hrefmap;
	struct rb_root frefmap;
	struct rb_root fhrefmap;
	struct rb_root fnrefmap;
	struct rb_root fdrefmap;
	struct rb_root fdhrefmap;
	struct rb_root fdnrefmap;
	struct rb_root grefmap;
	struct rb_root ghrefmap;
	struct rb_root gnrefmap;
	struct matrix_data* funcs_tree_calls_no_asm;
	struct matrix_data* funcs_tree_calls_no_known;
	struct matrix_data* funcs_tree_calls_no_known_no_asm;
	struct matrix_data* funcs_tree_func_calls;
	struct matrix_data* funcs_tree_func_refs;
	struct matrix_data* funcs_tree_funrefs_no_asm;
	struct matrix_data* funcs_tree_funrefs_no_known;
	struct matrix_data* funcs_tree_funrefs_no_known_no_asm;
	struct matrix_data* globs_tree_globalrefs;
	struct matrix_data* types_tree_refs;
	struct matrix_data* types_tree_usedrefs;
	struct func_map_entry* static_funcs_map;
	unsigned long static_funcs_map_count;
	struct init_data_entry* init_data;
	unsigned long init_data_count;
	struct known_data_entry* known_data;
	struct BAS_item* BAS_data;
	unsigned long BAS_data_count;
};

#endif /* __FTDB_H__ */
