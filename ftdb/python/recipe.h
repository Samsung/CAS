FUNCTION_DECLARE_FLATTEN_STRUCT(stringRefMap_node);

FUNCTION_DEFINE_FLATTEN_STRUCT(stringRefMap_node,
    AGGREGATE_FLATTEN_STRUCT_EMBEDDED_POINTER(stringRefMap_node,node.__rb_parent_color,
            ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
    AGGREGATE_FLATTEN_STRUCT(stringRefMap_node,node.rb_right);
    AGGREGATE_FLATTEN_STRUCT(stringRefMap_node,node.rb_left);
    AGGREGATE_FLATTEN_STRING(key);
);

FUNCTION_DECLARE_FLATTEN_STRUCT(ftdb);
FUNCTION_DECLARE_FLATTEN_STRUCT(ftdb_func_entry);
FUNCTION_DECLARE_FLATTEN_STRUCT(ftdb_funcdecl_entry);
FUNCTION_DECLARE_FLATTEN_STRUCT(ftdb_unresolvedfunc_entry);
FUNCTION_DECLARE_FLATTEN_STRUCT(taint_element);
FUNCTION_DECLARE_FLATTEN_STRUCT(taint_data);
FUNCTION_DECLARE_FLATTEN_STRUCT(call_info);
FUNCTION_DECLARE_FLATTEN_STRUCT(callref_info);
FUNCTION_DECLARE_FLATTEN_STRUCT(callref_data);
FUNCTION_DECLARE_FLATTEN_STRUCT(refcall);
FUNCTION_DECLARE_FLATTEN_STRUCT(switch_info);
FUNCTION_DECLARE_FLATTEN_STRUCT(case_info);
FUNCTION_DECLARE_FLATTEN_STRUCT(case_data);
FUNCTION_DECLARE_FLATTEN_STRUCT(csitem);
FUNCTION_DECLARE_FLATTEN_STRUCT(mexp_info);
FUNCTION_DECLARE_FLATTEN_STRUCT(local_info);
FUNCTION_DECLARE_FLATTEN_STRUCT(deref_info);
FUNCTION_DECLARE_FLATTEN_STRUCT(offsetref_info);
FUNCTION_DECLARE_FLATTEN_STRUCT(if_info);
FUNCTION_DECLARE_FLATTEN_STRUCT(ifref_info);
FUNCTION_DECLARE_FLATTEN_STRUCT(asm_info);
FUNCTION_DECLARE_FLATTEN_STRUCT(globalref_data);
FUNCTION_DECLARE_FLATTEN_STRUCT(globalref_info);
FUNCTION_DECLARE_FLATTEN_STRUCT(ftdb_global_entry);
FUNCTION_DECLARE_FLATTEN_STRUCT(ftdb_type_entry);
FUNCTION_DECLARE_FLATTEN_STRUCT(ftdb_fops_entry);
FUNCTION_DECLARE_FLATTEN_STRUCT(ftdb_fops_member_entry);
FUNCTION_DECLARE_FLATTEN_STRUCT(bitfield);
FUNCTION_DECLARE_FLATTEN_STRUCT(func_fptrs_item);

FUNCTION_DEFINE_FLATTEN_STRUCT(taint_data,
    AGGREGATE_FLATTEN_STRUCT_ARRAY(taint_element,taint_list,ATTR(taint_list_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(taint_element,
    /* No recipes needed */
);

FUNCTION_DEFINE_FLATTEN_STRUCT(call_info,
    AGGREGATE_FLATTEN_STRING(start);
    AGGREGATE_FLATTEN_STRING(end);
    AGGREGATE_FLATTEN_STRING(expr);
    AGGREGATE_FLATTEN_STRING(loc);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,args,ATTR(args_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(int64_t,csid,1);
);

FUNCTION_DEFINE_FLATTEN_STRUCT(callref_info,
    AGGREGATE_FLATTEN_STRUCT_ARRAY(callref_data,callarg,ATTR(callarg_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(callref_data,
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,id,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int64_t,integer_literal,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,character_literal,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(double,floating_literal,1);
    AGGREGATE_FLATTEN_STRING(string_literal);
);

FUNCTION_DEFINE_FLATTEN_STRUCT(refcall,
    /* No recipes needed */
);

FUNCTION_DEFINE_FLATTEN_STRUCT(switch_info,
    AGGREGATE_FLATTEN_STRING(condition);
    AGGREGATE_FLATTEN_STRUCT_ARRAY(case_info,cases,ATTR(cases_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(case_info,
    AGGREGATE_FLATTEN_STRING(lhs.enum_code);
    AGGREGATE_FLATTEN_STRING(lhs.macro_code);
    AGGREGATE_FLATTEN_STRING(lhs.raw_code);
    AGGREGATE_FLATTEN_STRUCT_ARRAY(case_data,rhs,1);
);

FUNCTION_DEFINE_FLATTEN_STRUCT(case_data,
    AGGREGATE_FLATTEN_STRING(enum_code);
    AGGREGATE_FLATTEN_STRING(macro_code);
    AGGREGATE_FLATTEN_STRING(raw_code);
);

FUNCTION_DEFINE_FLATTEN_STRUCT(csitem,
        AGGREGATE_FLATTEN_STRING(cf);
);

FUNCTION_DEFINE_FLATTEN_STRUCT(mexp_info,
        AGGREGATE_FLATTEN_STRING(text);
);

FUNCTION_DEFINE_FLATTEN_STRUCT(local_info,
    AGGREGATE_FLATTEN_STRING(name);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int64_t,csid,1);
    AGGREGATE_FLATTEN_STRING(location);
);

FUNCTION_DEFINE_FLATTEN_STRUCT(deref_info,
    AGGREGATE_FLATTEN_TYPE_ARRAY(int64_t,offset,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,basecnt,1);
    AGGREGATE_FLATTEN_STRUCT_ARRAY(offsetref_info,offsetrefs,ATTR(offsetrefs_count));
    AGGREGATE_FLATTEN_STRING(expr);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,ord,ATTR(ord_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(int64_t,member,ATTR(member_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,type,ATTR(type_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,access,ATTR(access_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(int64_t,shift,ATTR(shift_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(int64_t,mcall,ATTR(mcall_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(offsetref_info,
    if (ATTR(kind)==EXPR_STRING) {
        AGGREGATE_FLATTEN_STRING(id_string);
    }
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,mi,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,di,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,cast,1);
);

FUNCTION_DEFINE_FLATTEN_STRUCT(if_info,
    AGGREGATE_FLATTEN_STRUCT_ARRAY(ifref_info,refs,ATTR(refs_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(ifref_info,
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,id,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int64_t,integer_literal,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,character_literal,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(double,floating_literal,1);
    AGGREGATE_FLATTEN_STRING(string_literal);
);

FUNCTION_DEFINE_FLATTEN_STRUCT(asm_info,
    AGGREGATE_FLATTEN_STRING(str);
);

FUNCTION_DEFINE_FLATTEN_STRUCT(globalref_data,
    AGGREGATE_FLATTEN_STRUCT_ARRAY(globalref_info,refs,ATTR(refs_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(globalref_info,
    AGGREGATE_FLATTEN_STRING(start);
    AGGREGATE_FLATTEN_STRING(end);
);

FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE(ftdb_ulong_type_entryMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE(ftdb_ulong_func_entryMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE(ftdb_ulong_funcdecl_entryMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE(ftdb_ulong_global_entryMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE(ftdb_ulong_static_funcs_map_entryMap);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE(ftdb_ulong_type_entryMap,
    AGGREGATE_FLATTEN_STRUCT_TYPE_EMBEDDED_POINTER(ftdb_ulong_type_entryMap,node.__rb_parent_color,
            ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_type_entryMap,node.rb_right);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_type_entryMap,node.rb_left);
    AGGREGATE_FLATTEN_STRUCT(ftdb_type_entry,entry);
);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE(ftdb_ulong_func_entryMap,
    AGGREGATE_FLATTEN_STRUCT_TYPE_EMBEDDED_POINTER(ftdb_ulong_func_entryMap,node.__rb_parent_color,
            ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_func_entryMap,node.rb_right);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_func_entryMap,node.rb_left);
    AGGREGATE_FLATTEN_STRUCT(ftdb_func_entry,entry);
);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE(ftdb_ulong_funcdecl_entryMap,
    AGGREGATE_FLATTEN_STRUCT_TYPE_EMBEDDED_POINTER(ftdb_ulong_funcdecl_entryMap,node.__rb_parent_color,
            ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_funcdecl_entryMap,node.rb_right);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_funcdecl_entryMap,node.rb_left);
    AGGREGATE_FLATTEN_STRUCT(ftdb_funcdecl_entry,entry);
);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE(ftdb_ulong_global_entryMap,
    AGGREGATE_FLATTEN_STRUCT_TYPE_EMBEDDED_POINTER(ftdb_ulong_global_entryMap,node.__rb_parent_color,
            ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_global_entryMap,node.rb_right);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_global_entryMap,node.rb_left);
    AGGREGATE_FLATTEN_STRUCT(ftdb_global_entry,entry);
);

FUNCTION_DECLARE_FLATTEN_STRUCT(func_map_entry);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE(ftdb_ulong_static_funcs_map_entryMap,
    AGGREGATE_FLATTEN_STRUCT_TYPE_EMBEDDED_POINTER(ftdb_ulong_static_funcs_map_entryMap,node.__rb_parent_color,
            ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_static_funcs_map_entryMap,node.rb_right);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_static_funcs_map_entryMap,node.rb_left);
    AGGREGATE_FLATTEN_STRUCT(func_map_entry,entry);
);

FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_type_entryMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_func_entryMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_funcdecl_entryMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_global_entryMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_BAS_data_entryMap);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_type_entryMap,
    AGGREGATE_FLATTEN_STRUCT_TYPE_EMBEDDED_POINTER(ftdb_stringRef_type_entryMap,node.__rb_parent_color,
            ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_type_entryMap,node.rb_right);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_type_entryMap,node.rb_left);
    AGGREGATE_FLATTEN_STRING(key);
    AGGREGATE_FLATTEN_STRUCT(ftdb_type_entry,entry);
);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_func_entryMap,
    AGGREGATE_FLATTEN_STRUCT_TYPE_EMBEDDED_POINTER(ftdb_stringRef_func_entryMap,node.__rb_parent_color,
            ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_func_entryMap,node.rb_right);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_func_entryMap,node.rb_left);
    AGGREGATE_FLATTEN_STRING(key);
    AGGREGATE_FLATTEN_STRUCT(ftdb_func_entry,entry);
);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_funcdecl_entryMap,
    AGGREGATE_FLATTEN_STRUCT_TYPE_EMBEDDED_POINTER(ftdb_stringRef_funcdecl_entryMap,node.__rb_parent_color,
            ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_funcdecl_entryMap,node.rb_right);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_funcdecl_entryMap,node.rb_left);
    AGGREGATE_FLATTEN_STRING(key);
    AGGREGATE_FLATTEN_STRUCT(ftdb_funcdecl_entry,entry);
);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_global_entryMap,
    AGGREGATE_FLATTEN_STRUCT_TYPE_EMBEDDED_POINTER(ftdb_stringRef_global_entryMap,node.__rb_parent_color,
            ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_global_entryMap,node.rb_right);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_global_entryMap,node.rb_left);
    AGGREGATE_FLATTEN_STRING(key);
    AGGREGATE_FLATTEN_STRUCT(ftdb_global_entry,entry);
);

FUNCTION_DECLARE_FLATTEN_STRUCT(BAS_item);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_BAS_data_entryMap,
    AGGREGATE_FLATTEN_STRUCT_TYPE_EMBEDDED_POINTER(ftdb_stringRef_BAS_data_entryMap,node.__rb_parent_color,
            ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_BAS_data_entryMap,node.rb_right);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_BAS_data_entryMap,node.rb_left);
    AGGREGATE_FLATTEN_STRING(key);
    AGGREGATE_FLATTEN_STRUCT(BAS_item,entry);
);

FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_func_entryListMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_global_entryListMap);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_func_entryListMap,
    AGGREGATE_FLATTEN_STRUCT_TYPE_EMBEDDED_POINTER(ftdb_stringRef_func_entryListMap,node.__rb_parent_color,
            ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_func_entryListMap,node.rb_right);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_func_entryListMap,node.rb_left);
    AGGREGATE_FLATTEN_STRING(key);
    AGGREGATE_FLATTEN_TYPE_ARRAY(void*,entry_list,ATTR(entry_count));
    FOREACH_POINTER(void*,entry,ATTR(entry_list),ATTR(entry_count),
        FLATTEN_STRUCT(ftdb_func_entry,entry);
    );
);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_global_entryListMap,
    AGGREGATE_FLATTEN_STRUCT_TYPE_EMBEDDED_POINTER(ftdb_stringRef_global_entryListMap,node.__rb_parent_color,
            ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_global_entryListMap,node.rb_right);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_global_entryListMap,node.rb_left);
    AGGREGATE_FLATTEN_STRING(key);
    AGGREGATE_FLATTEN_TYPE_ARRAY(void*,entry_list,ATTR(entry_count));
    FOREACH_POINTER(void*,entry,ATTR(entry_list),ATTR(entry_count),
        FLATTEN_STRUCT(ftdb_global_entry,entry);
    );
);

FUNCTION_DEFINE_FLATTEN_STRUCT(matrix_data,
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,data,ATTR(data_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,row_ind,ATTR(row_ind_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,col_ind,ATTR(col_ind_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(func_map_entry,
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,fids,ATTR(fids_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(size_dep_item,
    // No recipes needed
);

FUNCTION_DECLARE_FLATTEN_STRUCT(init_data_item);

FUNCTION_DEFINE_FLATTEN_STRUCT(init_data_item,
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,id,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,name,ATTR(name_count));
    FOREACH_POINTER(const char*,s,ATTR(name),ATTR(name_count),
        FLATTEN_STRING(s);
    );
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,size,1);
    AGGREGATE_FLATTEN_STRUCT_ARRAY(size_dep_item,size_dep,1);
    AGGREGATE_FLATTEN_STRING(nullterminated);
    AGGREGATE_FLATTEN_STRING(tagged);
    AGGREGATE_FLATTEN_STRING(fuzz);
    AGGREGATE_FLATTEN_STRING(pointer);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,min_value,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,max_value,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,value,1);
    AGGREGATE_FLATTEN_STRING(user_name);
    AGGREGATE_FLATTEN_STRING(protected_var);
    AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,value_dep,ATTR(value_dep_count));
    FOREACH_POINTER(const char*,s,ATTR(value_dep),ATTR(value_dep_count),
        FLATTEN_STRING(s);
    );
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,fuzz_offset,1);
    AGGREGATE_FLATTEN_STRING(force_type);
    AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,always_init,ATTR(always_init_count));
    FOREACH_POINTER(const char*,s,ATTR(always_init),ATTR(always_init_count),
        FLATTEN_STRING(s);
    );
    AGGREGATE_FLATTEN_STRUCT_ARRAY(init_data_item,subitems,ATTR(subitems_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(init_data_entry,
    AGGREGATE_FLATTEN_STRING(name);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,order,ATTR(order_count));
    AGGREGATE_FLATTEN_STRING(interface);
    AGGREGATE_FLATTEN_STRUCT_ARRAY(init_data_item,items,ATTR(items_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(known_data_entry,
    AGGREGATE_FLATTEN_STRING(version);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,func_ids,ATTR(func_ids_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,builtin_ids,ATTR(builtin_ids_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,asm_ids,ATTR(asm_ids_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,lib_funcs,ATTR(lib_funcs_count));
    FOREACH_POINTER(const char*,s,ATTR(lib_funcs),ATTR(lib_funcs_count),
        FLATTEN_STRING(s);
    );
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,lib_funcs_ids,ATTR(lib_funcs_ids_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,always_inc_funcs_ids,ATTR(always_inc_funcs_ids_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,replacement_ids,ATTR(replacement_ids_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(BAS_item,
    AGGREGATE_FLATTEN_STRING(loc);
    AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,entries,ATTR(entries_count));
    FOREACH_POINTER(const char*,s,ATTR(entries),ATTR(entries_count),
        FLATTEN_STRING(s);
    );
);

FUNCTION_DEFINE_FLATTEN_STRUCT(ftdb,
    AGGREGATE_FLATTEN_STRUCT_ARRAY(ftdb_func_entry,funcs,ATTR(funcs_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(ftdb_funcdecl_entry,funcdecls,ATTR(funcdecls_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(ftdb_unresolvedfunc_entry,unresolvedfuncs,ATTR(unresolvedfuncs_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(ftdb_global_entry,globals,ATTR(globals_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(ftdb_type_entry,types,ATTR(types_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(ftdb_fops_entry,fops,ATTR(fops_count));
    AGGREGATE_FLATTEN_STRUCT(stringRefMap_node,sourcemap.rb_node);
    AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,sourceindex_table,ATTR(sourceindex_table_count));
    FOREACH_POINTER(const char*,s,ATTR(sourceindex_table),ATTR(sourceindex_table_count),
        FLATTEN_STRING(s);
    );
    AGGREGATE_FLATTEN_STRUCT(stringRefMap_node,modulemap.rb_node);
    AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,moduleindex_table,ATTR(moduleindex_table_count));
    FOREACH_POINTER(const char*,s,ATTR(moduleindex_table),ATTR(moduleindex_table_count),
        FLATTEN_STRING(s);
    );
    AGGREGATE_FLATTEN_STRUCT(stringRefMap_node,macromap.rb_node);
    AGGREGATE_FLATTEN_STRING(version);
    AGGREGATE_FLATTEN_STRING(module);
    AGGREGATE_FLATTEN_STRING(directory);
    AGGREGATE_FLATTEN_STRING(release);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_type_entryMap,refmap.rb_node);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_type_entryMap,hrefmap.rb_node);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_func_entryMap,frefmap.rb_node);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_func_entryMap,fhrefmap.rb_node);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_func_entryListMap,fnrefmap.rb_node);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_funcdecl_entryMap,fdrefmap.rb_node);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_funcdecl_entryMap,fdhrefmap.rb_node);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_func_entryListMap,fdnrefmap.rb_node);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_global_entryMap,grefmap.rb_node);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_global_entryMap,ghrefmap.rb_node);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_global_entryListMap,gnrefmap.rb_node);

    AGGREGATE_FLATTEN_STRUCT(matrix_data,funcs_tree_calls_no_asm);
    AGGREGATE_FLATTEN_STRUCT(matrix_data,funcs_tree_calls_no_known);
    AGGREGATE_FLATTEN_STRUCT(matrix_data,funcs_tree_calls_no_known_no_asm);
    AGGREGATE_FLATTEN_STRUCT(matrix_data,funcs_tree_func_calls);
    AGGREGATE_FLATTEN_STRUCT(matrix_data,funcs_tree_func_refs);
    AGGREGATE_FLATTEN_STRUCT(matrix_data,funcs_tree_funrefs_no_asm);
    AGGREGATE_FLATTEN_STRUCT(matrix_data,funcs_tree_funrefs_no_known);
    AGGREGATE_FLATTEN_STRUCT(matrix_data,funcs_tree_funrefs_no_known_no_asm);
    AGGREGATE_FLATTEN_STRUCT(matrix_data,globs_tree_globalrefs);
    AGGREGATE_FLATTEN_STRUCT(matrix_data,types_tree_refs);
    AGGREGATE_FLATTEN_STRUCT(matrix_data,types_tree_usedrefs);
    AGGREGATE_FLATTEN_STRUCT_ARRAY(func_map_entry,static_funcs_map,ATTR(static_funcs_map_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(init_data_entry,init_data,ATTR(init_data_count));
    AGGREGATE_FLATTEN_STRUCT(known_data_entry,known_data);
    AGGREGATE_FLATTEN_STRUCT_ARRAY(BAS_item,BAS_data,ATTR(BAS_data_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(func_fptrs_item,func_fptrs_data,ATTR(func_fptrs_data_count));

    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_ulong_static_funcs_map_entryMap,static_funcs_map_index.rb_node);
    AGGREGATE_FLATTEN_STRUCT_TYPE(ftdb_stringRef_BAS_data_entryMap,BAS_data_index.rb_node);
);

FUNCTION_DEFINE_FLATTEN_STRUCT(ftdb_func_entry,
    AGGREGATE_FLATTEN_STRING(name);
    AGGREGATE_FLATTEN_STRING(__namespace);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,fids,ATTR(fids_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,mids,ATTR(mids_count));
    AGGREGATE_FLATTEN_STRING(firstNonDeclStmt);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,isinline,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,istemplate,1);
    AGGREGATE_FLATTEN_STRING(classOuterFn);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,ismember,1);
    AGGREGATE_FLATTEN_STRING(__class);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,classid,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,attributes,ATTR(attributes_count));
    FOREACH_POINTER(const char*,s,ATTR(attributes),ATTR(attributes_count),
        FLATTEN_STRING(s);
    );
    AGGREGATE_FLATTEN_STRING(hash);
    AGGREGATE_FLATTEN_STRING(cshash);
    AGGREGATE_FLATTEN_STRING(template_parameters);
    AGGREGATE_FLATTEN_STRING(body);
    AGGREGATE_FLATTEN_STRING(unpreprocessed_body);
    AGGREGATE_FLATTEN_STRING(declbody);
    AGGREGATE_FLATTEN_STRING(signature);
    AGGREGATE_FLATTEN_STRING(declhash);
    AGGREGATE_FLATTEN_STRING(location);
    AGGREGATE_FLATTEN_STRING(start_loc);
    AGGREGATE_FLATTEN_STRING(end_loc);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int64_t,integer_literals,ATTR(integer_literals_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,character_literals,ATTR(character_literals_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(double,floating_literals,ATTR(floating_literals_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,string_literals,ATTR(string_literals_count));
    FOREACH_POINTER(const char*,s,ATTR(string_literals),ATTR(string_literals_count),
        FLATTEN_STRING(s);
    );
    AGGREGATE_FLATTEN_STRUCT_ARRAY(taint_data,taint,ATTR(taint_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,calls,ATTR(calls_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(call_info,call_info,ATTR(calls_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(callref_info,callrefs,ATTR(calls_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(refcall,refcalls,ATTR(refcalls_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(call_info,refcall_info,ATTR(refcalls_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(callref_info,refcallrefs,ATTR(refcalls_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(switch_info,switches,ATTR(switches_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(csitem,csmap,ATTR(csmap_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(mexp_info,macro_expansions,ATTR(macro_expansions_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(local_info,locals,ATTR(locals_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(deref_info,derefs,ATTR(derefs_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(if_info,ifs,ATTR(ifs_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(asm_info,asms,ATTR(asms_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,globalrefs,ATTR(globalrefs_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(globalref_data,globalrefInfo,ATTR(globalrefs_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,funrefs,ATTR(funrefs_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,refs,ATTR(refs_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,decls,ATTR(decls_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,types,ATTR(types_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(ftdb_funcdecl_entry,
    AGGREGATE_FLATTEN_STRING(name);
    AGGREGATE_FLATTEN_STRING(__namespace);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,istemplate,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,ismember,1);
    AGGREGATE_FLATTEN_STRING(__class);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,classid,1);
    AGGREGATE_FLATTEN_STRING(template_parameters);
    AGGREGATE_FLATTEN_STRING(decl);
    AGGREGATE_FLATTEN_STRING(signature);
    AGGREGATE_FLATTEN_STRING(declhash);
    AGGREGATE_FLATTEN_STRING(location);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,types,ATTR(types_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(ftdb_unresolvedfunc_entry,
    AGGREGATE_FLATTEN_STRING(name);
);

FUNCTION_DEFINE_FLATTEN_STRUCT(ftdb_global_entry,
    AGGREGATE_FLATTEN_STRING(name);
    AGGREGATE_FLATTEN_STRING(hash);
    AGGREGATE_FLATTEN_STRING(def);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,globalrefs,ATTR(globalrefs_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,refs,ATTR(refs_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,funrefs,ATTR(funrefs_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,decls,ATTR(decls_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,mids,ATTR(mids_count));
    AGGREGATE_FLATTEN_STRING(location);
    AGGREGATE_FLATTEN_STRING(init);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int64_t,integer_literals,ATTR(integer_literals_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,character_literals,ATTR(character_literals_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(double,floating_literals,ATTR(floating_literals_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,string_literals,ATTR(string_literals_count));
    FOREACH_POINTER(const char*,s,ATTR(string_literals),ATTR(string_literals_count),
        FLATTEN_STRING(s);
    );
);

FUNCTION_DEFINE_FLATTEN_STRUCT(bitfield,
    /* No recipes needed */
);

FUNCTION_DEFINE_FLATTEN_STRUCT(ftdb_type_entry,
    AGGREGATE_FLATTEN_STRING(hash);
    AGGREGATE_FLATTEN_STRING(class_name);
    AGGREGATE_FLATTEN_STRING(qualifiers);
    AGGREGATE_FLATTEN_STRING(str);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,refs,ATTR(refs_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,usedrefs,ATTR(usedrefs_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,decls,ATTR(decls_count));
    AGGREGATE_FLATTEN_STRING(def);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,memberoffsets,ATTR(memberoffsets_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,attrrefs,ATTR(attrrefs_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,attrnum,1);
    AGGREGATE_FLATTEN_STRING(name);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,isdependent,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,globalrefs,ATTR(globalrefs_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(int64_t,enumvalues,ATTR(enumvalues_count));
    AGGREGATE_FLATTEN_STRING(outerfn);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,outerfnid,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,isimplicit,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,isunion,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,refnames,ATTR(refnames_count));
    FOREACH_POINTER(const char*,s,ATTR(refnames),ATTR(refnames_count),
        FLATTEN_STRING(s);
    );
    AGGREGATE_FLATTEN_STRING(location);
    AGGREGATE_FLATTEN_TYPE_ARRAY(int,isvariadic,1);
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,funrefs,ATTR(funrefs_count));
    AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,useddef,ATTR(useddef_count));
    FOREACH_POINTER(const char*,s,ATTR(useddef),ATTR(useddef_count),
        FLATTEN_STRING(s);
    );
    AGGREGATE_FLATTEN_STRING(defhead);
    AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,identifiers,ATTR(identifiers_count));
    FOREACH_POINTER(const char*,s,ATTR(identifiers),ATTR(identifiers_count),
        FLATTEN_STRING(s);
    );
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,methods,ATTR(methods_count));
    AGGREGATE_FLATTEN_STRUCT_ARRAY(bitfield,bitfields,ATTR(bitfields_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(ftdb_fops_member_entry,
    AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,func_ids,ATTR(func_ids_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(ftdb_fops_entry,
    AGGREGATE_FLATTEN_STRING(location);
    AGGREGATE_FLATTEN_STRUCT_ARRAY(ftdb_fops_member_entry,members,ATTR(members_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(func_fptrs_entry,
    AGGREGATE_FLATTEN_STRING(fname);
    AGGREGATE_FLATTEN_TYPE_ARRAY(long*,ids,ATTR(ids_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(func_fptrs_item,
    AGGREGATE_FLATTEN_STRUCT_ARRAY(func_fptrs_entry,entries,ATTR(entries_count));
);
