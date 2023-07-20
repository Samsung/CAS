#ifndef __FTDB_ENTRY_H__
#define __FTDB_ENTRY_H__

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

#define FTDB_ENTRY_PYOBJECT(__entry,__key) \
        ({      \
                PyObject* key_##__key = PyUnicode_FromString(#__key);   \
                PyObject* __key = PyDict_GetItem(__entry, key_##__key); \
                Py_DecRef(key_##__key); \
                __key;    \
        })

#ifdef DEBUG
#define FTDB_ENTRY_STRING(__entry,__key) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		PyObject* __key = PyDict_GetItem(__entry, key_##__key);	\
		const char* __new_entry = PyString_get_c_str(__key);	\
		if (!__new_entry) {	\
			printf("Cannot convert Unicode to c-string at entry with key \"" #__key "\"\n");	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})
#else
#define FTDB_ENTRY_STRING(__entry,__key) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		PyObject* __key = PyDict_GetItem(__entry, key_##__key);	\
		const char* __new_entry = PyString_get_c_str(__key);	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})
#endif

#ifdef DEBUG
#define FTDB_ENTRY_STRING_OPTIONAL(__entry,__key) \
	({	\
		const char* __new_entry = 0;	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		if (PyDict_Contains(__entry, key_##__key)) {	\
			PyObject* __key = PyDict_GetItem(__entry, key_##__key);	\
			__new_entry = PyString_get_c_str(__key);	\
			if (!__new_entry) {	\
				printf("Cannot convert Unicode to c-string at entry with key \"" #__key "\"\n");	\
			}	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})
#else
#define FTDB_ENTRY_STRING_OPTIONAL(__entry,__key) \
	({	\
		const char* __new_entry = 0;	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		if (PyDict_Contains(__entry, key_##__key)) {	\
			PyObject* __key = PyDict_GetItem(__entry, key_##__key);	\
			__new_entry = PyString_get_c_str(__key);	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})
#endif

#define FTDB_ENTRY_ULONG(__entry,__key) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		unsigned long __new_entry = PyLong_AsUnsignedLong(PyDict_GetItem(__entry,key_##__key));	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})

#define FTDB_ENTRY_ULONG_OPTIONAL(__entry,__key) \
	({	\
		unsigned long* __new_entry = 0;	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		if (PyDict_Contains(__entry, key_##__key)) {	\
			__new_entry = (unsigned long*)malloc(sizeof(unsigned long));	\
			*__new_entry = PyLong_AsUnsignedLong(PyDict_GetItem(__entry,key_##__key));	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})

#define FTDB_ENTRY_FLOAT(__entry,__key) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		double __new_entry = PyFloat_AsDouble(PyDict_GetItem(__entry,key_##__key));	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})

#define FTDB_ENTRY_FLOAT_OPTIONAL(__entry,__key) \
	({	\
		double* __new_entry = 0;	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		if (PyDict_Contains(__entry, key_##__key)) {	\
			__new_entry = (double*)malloc(sizeof(double));	\
			*__new_entry = PyFloat_AsDouble(PyDict_GetItem(__entry,key_##__key));	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})

#define FTDB_ENTRY_INT64(__entry,__key) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		int64_t __new_entry = PyLong_AsLong(PyDict_GetItem(__entry,key_##__key));	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})

#define FTDB_ENTRY_INT64_OPTIONAL(__entry,__key) \
	({	\
		int64_t* __new_entry = 0;	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		if (PyDict_Contains(__entry, key_##__key)) {	\
			__new_entry = (int64_t*)malloc(sizeof(int64_t));	\
			*__new_entry = PyLong_AsLong(PyDict_GetItem(__entry,key_##__key));	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})

#define FTDB_ENTRY_INT(__entry,__key) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		int __new_entry = PyLong_AsLong(PyDict_GetItem(__entry,key_##__key));	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})

#define FTDB_ENTRY_INT_OPTIONAL(__entry,__key) \
	({	\
		int* __new_entry = 0;	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		if (PyDict_Contains(__entry, key_##__key)) {	\
			__new_entry = (int*)malloc(sizeof(int));	\
			*__new_entry = PyLong_AsLong(PyDict_GetItem(__entry,key_##__key));	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})

#define FTDB_ENTRY_ENUM_FROM_STRING(__entry,__key,__enum) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		PyObject* __key = PyDict_GetItem(__entry, key_##__key);	\
		enum __enum __new_entry = get_##__enum(PyString_get_c_str(__key));	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})

#define FTDB_ENTRY_ENUM_FROM_ULONG(__entry,__key,__enum) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		PyObject* __key = PyDict_GetItem(__entry, key_##__key);	\
		enum __enum __new_entry = get_##__enum(PyLong_AsUnsignedLong(__key));	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})

#define FTDB_ENTRY_BOOL(__entry,__key) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		int __new_entry=0;	\
		PyObject* o = PyDict_GetItem(__entry,key_##__key);	\
		if (PyBool_Check(o)) {	\
			__new_entry = PyObject_IsTrue(o);	\
		}	\
		else if (PyLong_Check(o)) {	\
			__new_entry = PyLong_AsLong(o)!=0;	\
		}	\
		else {	\
			PyErr_SetString(libftdb_ftdbError,"ERROR: Invalid type for bool entry (not a bool or integer)");	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})

#define FTDB_ENTRY_BOOL_OPTIONAL(__entry,__key) \
	({	\
		int* __new_entry = 0;	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		if (PyDict_Contains(__entry, key_##__key)) {	\
			__new_entry = (int*)malloc(sizeof(int));	\
			PyObject* o = PyDict_GetItem(__entry,key_##__key);	\
			if (PyBool_Check(o)) {	\
				*__new_entry = PyObject_IsTrue(o);	\
			}	\
			else if (PyLong_Check(o)) {	\
				*__new_entry = PyLong_AsLong(o)!=0;	\
			}	\
			else {	\
				PyErr_SetString(libftdb_ftdbError,"ERROR: Invalid type for bool entry (not a bool or integer)");	\
			}	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})

#define FTDB_ENTRY_ARRAY_SIZE(__entry,__key) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		PyObject* __key = PyDict_GetItem(__entry,key_##__key);	\
		unsigned long __key##_count = PyList_Size(__key);	\
		Py_DecRef(key_##__key);	\
		__key##_count;	\
	})

#define FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(__entry,__key) \
	({	\
		unsigned long __key##_count = 0;	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		if (PyDict_Contains(__entry, key_##__key)) {	\
			PyObject* __key = PyDict_GetItem(__entry,key_##__key);	\
			__key##_count = PyList_Size(__key);	\
		}	\
		Py_DecRef(key_##__key);	\
		__key##_count;	\
	})

#define FTDB_ENTRY_ULONG_ARRAY(__entry,__key) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		PyObject* __key = PyDict_GetItem(__entry,key_##__key);	\
		unsigned long __key##_count = PyList_Size(__key);	\
		unsigned long* __new_array = calloc(__key##_count,sizeof(unsigned long));	\
		for (unsigned long __i=0; __i<__key##_count; ++__i) {	\
			__new_array[__i] = PyLong_AsUnsignedLong(PyList_GetItem(__key,__i));	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_array;	\
	})

#define FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(__entry,__key) \
	({	\
		unsigned long* __new_array = 0;	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		if (PyDict_Contains(__entry, key_##__key)) {	\
			PyObject* __key = PyDict_GetItem(__entry,key_##__key);	\
			unsigned long __key##_count = PyList_Size(__key);	\
			__new_array = calloc(__key##_count,sizeof(unsigned long));	\
			for (unsigned long __i=0; __i<__key##_count; ++__i) {	\
				__new_array[__i] = PyLong_AsUnsignedLong(PyList_GetItem(__key,__i));	\
			}	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_array;	\
	})

#define FTDB_ENTRY_INT64_ARRAY(__entry,__key) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		PyObject* __key = PyDict_GetItem(__entry,key_##__key);	\
		unsigned long __key##_count = PyList_Size(__key);	\
		int64_t* __new_array = calloc(__key##_count,sizeof(int64_t));	\
		for (unsigned long __i=0; __i<__key##_count; ++__i) {	\
			__new_array[__i] = PyLong_AsLong(PyList_GetItem(__key,__i));	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_array;	\
	})

#define FTDB_ENTRY_INT64_ARRAY_OPTIONAL(__entry,__key) \
	({	\
		int64_t* __new_array = 0;	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		if (PyDict_Contains(__entry, key_##__key)) {	\
			PyObject* __key = PyDict_GetItem(__entry,key_##__key);	\
			unsigned long __key##_count = PyList_Size(__key);	\
			__new_array = calloc(__key##_count,sizeof(int64_t));	\
			for (unsigned long __i=0; __i<__key##_count; ++__i) {	\
				__new_array[__i] = PyLong_AsLong(PyList_GetItem(__key,__i));	\
			}	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_array;	\
	})

#define FTDB_ENTRY_INT_ARRAY(__entry,__key) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		PyObject* __key = PyDict_GetItem(__entry,key_##__key);	\
		unsigned long __key##_count = PyList_Size(__key);	\
		int* __new_array = calloc(__key##_count,sizeof(int));	\
		for (unsigned long __i=0; __i<__key##_count; ++__i) {	\
			__new_array[__i] = PyLong_AsLong(PyList_GetItem(__key,__i));	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_array;	\
	})

#define FTDB_ENTRY_UINT_ARRAY(__entry,__key) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		PyObject* __key = PyDict_GetItem(__entry,key_##__key);	\
		unsigned long __key##_count = PyList_Size(__key);	\
		unsigned int* __new_array = calloc(__key##_count,sizeof(unsigned int));	\
		for (unsigned long __i=0; __i<__key##_count; ++__i) {	\
			__new_array[__i] = PyLong_AsUnsignedLong(PyList_GetItem(__key,__i));	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_array;	\
	})

#define FTDB_ENTRY_FLOAT_ARRAY(__entry,__key) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		PyObject* __key = PyDict_GetItem(__entry,key_##__key);	\
		unsigned long __key##_count = PyList_Size(__key);	\
		double* __new_array = calloc(__key##_count,sizeof(double));	\
		for (unsigned long __i=0; __i<__key##_count; ++__i) {	\
			__new_array[__i] = PyFloat_AsDouble(PyList_GetItem(__key,__i));	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_array;	\
	})

#define FTDB_ENTRY_STRING_ARRAY(__entry,__key) \
	({	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		PyObject* __key = PyDict_GetItem(__entry,key_##__key);	\
		unsigned long __key##_count = PyList_Size(__key);	\
		const char** __new_array = calloc(__key##_count,sizeof(const char*));	\
		for (unsigned long __i=0; __i<__key##_count; ++__i) {	\
			__new_array[__i] = PyString_get_c_str(PyList_GetItem(__key,__i));	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_array;	\
	})

#define FTDB_ENTRY_STRING_ARRAY_OPTIONAL(__entry,__key) \
	({	\
		const char** __new_array = 0;	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		if (PyDict_Contains(__entry, key_##__key)) {	\
			PyObject* __key = PyDict_GetItem(__entry,key_##__key);	\
			unsigned long __key##_count = PyList_Size(__key);	\
			__new_array = calloc(__key##_count,sizeof(const char*));	\
			for (unsigned long __i=0; __i<__key##_count; ++__i) {	\
				__new_array[__i] = PyString_get_c_str(PyList_GetItem(__key,__i));	\
			}	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_array;	\
	})

#define FTDB_ENTRY_TYPE_OPTIONAL(__entry,__key,__typename) \
	({	\
		struct __typename* __new_entry = 0;	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		if (PyDict_Contains(__entry, key_##__key)) {	\
			PyObject* __key = PyDict_GetItem(__entry, key_##__key);	\
			__new_entry = calloc(1,sizeof(struct __typename));	\
			fill_##__typename##_entry(__key,__new_entry);	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_entry;	\
	})

#define FTDB_ENTRY_TYPE_ARRAY_OPTIONAL(__entry,__key,__typename,__count) \
	({	\
		struct __typename* __new_array = 0;	\
		PyObject* key_##__key = PyUnicode_FromString(#__key);	\
		if (PyDict_Contains(__entry, key_##__key)) {	\
			PyObject* __key = PyDict_GetItem(__entry,key_##__key);	\
			__new_array = calloc(__count,sizeof(struct __typename));	\
			for (unsigned long __i=0; __i<__count; ++__i) {	\
				fill_##__typename##_entry(PyList_GetItem(__key,__i),&__new_array[__i]);	\
			}	\
		}	\
		Py_DecRef(key_##__key);	\
		__new_array;	\
	})

#define FTDB_SET_ENTRY_STRING(__json,__name,__node)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyObject* val_##__name = PyUnicode_FromString(__node);	\
		PyDict_SetItem(__json,key_##__name,val_##__name);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(val_##__name);	\
	} while(0)

#define FTDB_SET_ENTRY_STRING_OPTIONAL(__json,__name,__node)	\
	do {	\
		if (__node) {	\
			PyObject* key_##__name = PyUnicode_FromString(#__name);	\
			PyObject* val_##__name = PyUnicode_FromString(__node);	\
			PyDict_SetItem(__json,key_##__name,val_##__name);	\
			Py_DecRef(key_##__name);	\
			Py_DecRef(val_##__name);	\
		}	\
	} while(0)

#define FTDB_SET_ENTRY_ULONG(__json,__name,__node)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyObject* val_##__name = PyLong_FromUnsignedLong(__node);	\
		PyDict_SetItem(__json,key_##__name,val_##__name);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(val_##__name);	\
	} while(0)

#define FTDB_SET_ENTRY_ULONG_OPTIONAL(__json,__name,__node)	\
	do {	\
		if (__node) {	\
			PyObject* key_##__name = PyUnicode_FromString(#__name);	\
			PyObject* val_##__name = PyLong_FromUnsignedLong(*__node);	\
			PyDict_SetItem(__json,key_##__name,val_##__name);	\
			Py_DecRef(key_##__name);	\
			Py_DecRef(val_##__name);	\
		}	\
	} while(0)

#define FTDB_SET_ENTRY_INT64(__json,__name,__node)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyObject* val_##__name = PyLong_FromLong(__node);	\
		PyDict_SetItem(__json,key_##__name,val_##__name);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(val_##__name);	\
	} while(0)

#define FTDB_SET_ENTRY_INT64_OPTIONAL(__json,__name,__node)	\
	do {	\
		if (__node) {	\
			PyObject* key_##__name = PyUnicode_FromString(#__name);	\
			PyObject* val_##__name = PyLong_FromLong(* __node);	\
			PyDict_SetItem(__json,key_##__name,val_##__name);	\
			Py_DecRef(key_##__name);	\
			Py_DecRef(val_##__name);	\
		}	\
	} while(0)

#define FTDB_SET_ENTRY_INT(__json,__name,__node)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyObject* val_##__name = PyLong_FromLong((int)__node);	\
		PyDict_SetItem(__json,key_##__name,val_##__name);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(val_##__name);	\
	} while(0)

#define FTDB_SET_ENTRY_UINT(__json,__name,__node)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyObject* val_##__name = PyLong_FromUnsignedLong((unsigned int)__node);	\
		PyDict_SetItem(__json,key_##__name,val_##__name);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(val_##__name);	\
	} while(0)

#define FTDB_SET_ENTRY_INT_OPTIONAL(__json,__name,__node)	\
	do {	\
		if (__node) {	\
			PyObject* key_##__name = PyUnicode_FromString(#__name);	\
			PyObject* val_##__name = PyLong_FromLong((int)(*__node));	\
			PyDict_SetItem(__json,key_##__name,val_##__name);	\
			Py_DecRef(key_##__name);	\
			Py_DecRef(val_##__name);	\
		}	\
	} while(0)

#define FTDB_SET_ENTRY_UINT_OPTIONAL(__json,__name,__node)	\
	do {	\
		if (__node) {	\
			PyObject* key_##__name = PyUnicode_FromString(#__name);	\
			PyObject* val_##__name = PyLong_FromUnsignedLong((unsigned int)(*__node));	\
			PyDict_SetItem(__json,key_##__name,val_##__name);	\
			Py_DecRef(key_##__name);	\
			Py_DecRef(val_##__name);	\
		}	\
	} while(0)

#define FTDB_SET_ENTRY_FLOAT(__json,__name,__node)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyObject* val_##__name = PyFloat_FromDouble(__node);	\
		PyDict_SetItem(__json,key_##__name,val_##__name);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(val_##__name);	\
	} while(0)

#define FTDB_SET_ENTRY_FLOAT_OPTIONAL(__json,__name,__node)	\
	do {	\
		if (__node) {	\
			PyObject* key_##__name = PyUnicode_FromString(#__name);	\
			PyObject* val_##__name = PyFloat_FromDouble(*__node);	\
			PyDict_SetItem(__json,key_##__name,val_##__name);	\
			Py_DecRef(key_##__name);	\
			Py_DecRef(val_##__name);	\
		}	\
	} while(0)

#define FTDB_SET_ENTRY_ULONG_ARRAY(__json,__name,__node)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyObject* py_##__name = PyList_New(0);	\
		for (unsigned long __i=0; __i<__node##_count; ++__i) {	\
			PyObject* val_##__name = PyLong_FromUnsignedLong(__node[__i]);	\
			PyList_Append(py_##__name,val_##__name);	\
			Py_DecRef(val_##__name);	\
		}	\
		PyDict_SetItem(__json,key_##__name,py_##__name);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(py_##__name);	\
	} while(0)

#define FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(__json,__name,__node)	\
	do {	\
		if (__node) {	\
			PyObject* key_##__name = PyUnicode_FromString(#__name);	\
			PyObject* py_##__name = PyList_New(0);	\
			for (unsigned long __i=0; __i<__node##_count; ++__i) {	\
				PyObject* val_##__name = PyLong_FromUnsignedLong(__node[__i]);	\
				PyList_Append(py_##__name,val_##__name);	\
				Py_DecRef(val_##__name);	\
			}	\
			PyDict_SetItem(__json,key_##__name,py_##__name);	\
			Py_DecRef(key_##__name);	\
			Py_DecRef(py_##__name);	\
		}	\
	} while(0)

#define FTDB_SET_ENTRY_INT64_ARRAY(__json,__name,__node)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyObject* py_##__name = PyList_New(0);	\
		for (unsigned long __i=0; __i<__node##_count; ++__i) {	\
			PyObject* val_##__name = PyLong_FromLong(__node[__i]);	\
			PyList_Append(py_##__name,val_##__name);	\
			Py_DecRef(val_##__name);	\
		}	\
		PyDict_SetItem(__json,key_##__name,py_##__name);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(py_##__name);	\
	} while(0)

#define FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(__json,__name,__node)	\
	do {	\
		if (__node) {	\
			PyObject* key_##__name = PyUnicode_FromString(#__name);	\
			PyObject* py_##__name = PyList_New(0);	\
			for (unsigned long __i=0; __i<__node##_count; ++__i) {	\
				PyObject* val_##__name = PyLong_FromLong(__node[__i]);	\
				PyList_Append(py_##__name,val_##__name);	\
				Py_DecRef(val_##__name);	\
			}	\
			PyDict_SetItem(__json,key_##__name,py_##__name);	\
			Py_DecRef(key_##__name);	\
			Py_DecRef(py_##__name);	\
		}	\
	} while(0)

#define FTDB_SET_ENTRY_INT_ARRAY(__json,__name,__node)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyObject* py_##__name = PyList_New(0);	\
		for (unsigned long __i=0; __i<__node##_count; ++__i) {	\
			PyObject* val_##__name = PyLong_FromLong(__node[__i]);	\
			PyList_Append(py_##__name,val_##__name);	\
			Py_DecRef(val_##__name);	\
		}	\
		PyDict_SetItem(__json,key_##__name,py_##__name);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(py_##__name);	\
	} while(0)

#define FTDB_SET_ENTRY_UINT_ARRAY(__json,__name,__node)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyObject* py_##__name = PyList_New(0);	\
		for (unsigned long __i=0; __i<__node##_count; ++__i) {	\
			PyObject* val_##__name = PyLong_FromUnsignedLong(__node[__i]);	\
			PyList_Append(py_##__name,val_##__name);	\
			Py_DecRef(val_##__name);	\
		}	\
		PyDict_SetItem(__json,key_##__name,py_##__name);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(py_##__name);	\
	} while(0)

#define FTDB_SET_ENTRY_INT_ARRAY_OPTIONAL(__json,__name,__node)	\
	do {	\
		if (__node) {	\
			PyObject* key_##__name = PyUnicode_FromString(#__name);	\
			PyObject* py_##__name = PyList_New(0);	\
			for (unsigned long __i=0; __i<__node##_count; ++__i) {	\
				PyObject* val_##__name = PyLong_FromLong(__node[__i]);	\
				PyList_Append(py_##__name,val_##__name);	\
				Py_DecRef(val_##__name);	\
			}	\
			PyDict_SetItem(__json,key_##__name,py_##__name);	\
			Py_DecRef(key_##__name);	\
			Py_DecRef(py_##__name);	\
		}	\
	} while(0)

#define FTDB_SET_ENTRY_UINT_ARRAY_OPTIONAL(__json,__name,__node)	\
	do {	\
		if (__node) {	\
			PyObject* key_##__name = PyUnicode_FromString(#__name);	\
			PyObject* py_##__name = PyList_New(0);	\
			for (unsigned long __i=0; __i<__node##_count; ++__i) {	\
				PyObject* val_##__name = PyLong_FromUnsignedLong(__node[__i]);	\
				PyList_Append(py_##__name,val_##__name);	\
				Py_DecRef(val_##__name);	\
			}	\
			PyDict_SetItem(__json,key_##__name,py_##__name);	\
			Py_DecRef(key_##__name);	\
			Py_DecRef(py_##__name);	\
		}	\
	} while(0)

#define FTDB_SET_ENTRY_FLOAT_ARRAY(__json,__name,__node)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyObject* py_##__name = PyList_New(0);	\
		for (unsigned long __i=0; __i<__node##_count; ++__i) {	\
			PyObject* val_##__name = PyFloat_FromDouble(__node[__i]);	\
			PyList_Append(py_##__name,val_##__name);	\
			Py_DecRef(val_##__name);	\
		}	\
		PyDict_SetItem(__json,key_##__name,py_##__name);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(py_##__name);	\
	} while(0)

#define FTDB_SET_ENTRY_FLOAT_ARRAY_OPTIONAL(__json,__name,__node)	\
	do {	\
		if (__node) {	\
			PyObject* key_##__name = PyUnicode_FromString(#__name);	\
			PyObject* py_##__name = PyList_New(0);	\
			for (unsigned long __i=0; __i<__node##_count; ++__i) {	\
				PyObject* val_##__name = PyFloat_FromDouble(__node[__i]);	\
				PyList_Append(py_##__name,val_##__name);	\
				Py_DecRef(val_##__name);	\
			}	\
			PyDict_SetItem(__json,key_##__name,py_##__name);	\
			Py_DecRef(key_##__name);	\
			Py_DecRef(py_##__name);	\
		}	\
	} while(0)

#define FTDB_SET_ENTRY_BOOL(__json,__name,__node)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyObject* val_##__name = PyBool_FromLong(__node); \
		PyDict_SetItem(__json,key_##__name,val_##__name);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(val_##__name);	\
	} while(0)

#define FTDB_SET_ENTRY_BOOL_OPTIONAL(__json,__name,__node)	\
	do {	\
		if (__node) {	\
			PyObject* key_##__name = PyUnicode_FromString(#__name);	\
			PyObject* val_##__name = PyBool_FromLong(*__node); \
			PyDict_SetItem(__json,key_##__name,val_##__name);	\
			Py_DecRef(key_##__name);	\
			Py_DecRef(val_##__name);	\
		}	\
	} while(0)

#define FTDB_SET_ENTRY_ENUM_TO_STRING(__json,__name,__node,__enum)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyObject* val_##__name = PyUnicode_FromString(set_##__enum(__node));	\
		PyDict_SetItem(__json,key_##__name,val_##__name);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(val_##__name);	\
	} while(0)

#define FTDB_SET_ENTRY_ENUM_TO_STRING_OPTIONAL(__json,__name,__node)	\
	do {	\
		if (__node) {	\
			PyObject* key_##__name = PyUnicode_FromString(#__name);	\
			PyObject* val_##__name = PyUnicode_FromString(set_##__enum(__node));	\
			PyDict_SetItem(__json,key_##__name,val_##__name);	\
			Py_DecRef(key_##__name);	\
			Py_DecRef(val_##__name);	\
		}	\
	} while(0)

#define FTDB_SET_ENTRY_STRING_ARRAY(__json,__name,__node)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyObject* py_##__name = PyList_New(0);	\
		for (unsigned long __i=0; __i<__node##_count; ++__i) {	\
			PyObject* __s = PyUnicode_FromString(__node[__i]);	\
			PyList_Append(py_##__name,__s);	\
			Py_DecRef(__s);	\
		}	\
		PyDict_SetItem(__json,key_##__name,py_##__name);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(py_##__name);	\
	} while(0)

#define FTDB_SET_ENTRY_STRING_ARRAY_OPTIONAL(__json,__name,__node)	\
	do {	\
		if (__node) {	\
			PyObject* key_##__name = PyUnicode_FromString(#__name);	\
			PyObject* py_##__name = PyList_New(0);	\
			for (unsigned long __i=0; __i<__node##_count; ++__i) {	\
				PyObject* __s = PyUnicode_FromString(__node[__i]);	\
				PyList_Append(py_##__name,__s);	\
				Py_DecRef(__s);	\
			}	\
			PyDict_SetItem(__json,key_##__name,py_##__name);	\
			Py_DecRef(key_##__name);	\
			Py_DecRef(py_##__name);	\
		}	\
	} while(0)

#define FTDB_SET_ENTRY_PYOBJECT(__json,__name,__node)	\
	do {	\
		PyObject* key_##__name = PyUnicode_FromString(#__name);	\
		PyDict_SetItem(__json,key_##__name,__node);	\
		Py_DecRef(key_##__name);	\
		Py_DecRef(__node);	\
	} while(0)

#define PYLIST_ADD_ULONG(__list,__node)	do {\
	PyObject* __val = PyLong_FromUnsignedLong(__node);	\
	PyList_Append(__list,__val);	\
	Py_DECREF(__val);	\
} while(0)

#define PYLIST_ADD_LONG(__list,__node)	do {\
	PyObject* __val = PyLong_FromLong(__node);	\
	PyList_Append(__list,__val);	\
	Py_DECREF(__val);	\
} while(0)

#define PYLIST_ADD_STRING(__list,__node)	do {\
	PyObject* __s = PyUnicode_FromString(__node);	\
	PyList_Append(__list,__s);	\
	Py_DECREF(__s);	\
} while(0)

#define PYLIST_ADD_FLOAT(__list,__node)	do {\
	PyObject* __val = PyFloat_FromDouble(__node);	\
	PyList_Append(__list,__val);	\
	Py_DECREF(__val);	\
} while(0)

#define PYLIST_ADD_PYOBJECT(__list,__node)	do {\
	PyList_Append(__list,__node);	\
	Py_DECREF(__node);	\
} while(0)

#define PYSET_ADD_PYOBJECT(__set,__node)	do {\
	if (!PySet_Contains(__set,__node)) {	\
		if (!PySet_Add(__set,__node)) {	\
			Py_DECREF(__node);	\
		}	\
	}	\
} while(0)

#define PYTUPLE_SET_ULONG(__tuple,__index,__node)	do {\
	PyObject* __val = PyLong_FromUnsignedLong(__node);	\
	PyTuple_SetItem(__tuple,__index,__val);	\
} while(0)

#define PYTUPLE_SET_LONG(__tuple,__index,__node)	do {\
	PyObject* __val = PyLong_FromLong(__node);	\
	PyTuple_SetItem(__tuple,__index,__val);	\
} while(0)

#define PYTUPLE_SET_STR(__tuple,__index,__node)	do {\
	PyObject* __val = PyUnicode_FromString(__node);	\
	PyTuple_SetItem(__tuple,__index,__val);	\
} while(0)

#define PYASSTR_DECREF(__node) do { \
	Py_DecRef((PyObject*)container_of(__node,PyBytesObject,ob_sval));	\
} while(0)

#define VALIDATE_FTDB_ENTRY(__db,__key)	\
do {	\
	PyObject* key_##__key = PyUnicode_FromString(#__key);	\
	if (!PyDict_Contains(__db, key_##__key)) {	\
		Py_DecRef(key_##__key);	\
		PyErr_SetString(libftdb_ftdbError,"ERROR: Invalid FTDB database format");	\
		Py_RETURN_FALSE;	\
	}	\
} while(0)

#endif /* __FTDB_ENTRY_H__ */
