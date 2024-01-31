#include <stdio.h>
#include <stdlib.h>
#include "openssl/sha.h"
#include "ftdb.h"

int main(int argc, char** argv) {

    if (argc<=1) {
        fprintf(stderr,"Usage: ./ftdb_sha256_test <PATH_to_FTDB_database>\n");
        return EXIT_FAILURE;
    }

    CFtdb ftdb_c = libftdb_c_ftdb_load(argv[1], 0, 0);
    if (!ftdb_c) {
        return EXIT_FAILURE;
    }

    struct ftdb* ftdb = libftdb_c_ftdb_object(ftdb_c);

    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    unsigned char digest[SHA256_DIGEST_LENGTH];

    for (unsigned long i=0; i<ftdb->funcs_count; ++i) {
        struct ftdb_func_entry* func_entry = &ftdb->funcs[i];
        SHA256_Update(&ctx,(const unsigned char*)func_entry->body,strlen(func_entry->body));
        SHA256_Update(&ctx,(const unsigned char*)func_entry->unpreprocessed_body,strlen(func_entry->unpreprocessed_body));
        for (unsigned long j=0; j<func_entry->derefs_count; ++j) {
            struct deref_info* deref_info = &func_entry->derefs[j];
            uint64_t deref_kind = deref_info->kind;
            SHA256_Update(&ctx,(const unsigned char*)&deref_kind,sizeof(uint64_t) );
        }
        for (unsigned long j=0; j<func_entry->types_count; ++j) {
            unsigned long type_id = func_entry->types[j];
            struct ftdb_type_entry* type_entry = libftdb_c_get_type_entry_by_id(ftdb_c, type_id);
            if (type_entry->def) {
                SHA256_Update(&ctx,(const unsigned char*)type_entry->def,strlen(type_entry->def));
            }
        }
    }
    for (unsigned long i=0; i<ftdb->globals_count; ++i) {
        struct ftdb_global_entry* global_entry = &ftdb->globals[i];
        unsigned long type_id = global_entry->type;
        struct ftdb_type_entry* type_entry = libftdb_c_get_type_entry_by_id(ftdb_c, type_id);
        if (type_entry->def) {
            SHA256_Update(&ctx,(const unsigned char*)type_entry->def,strlen(type_entry->def) );
        }
    }

    SHA256_Final(digest,&ctx);

    for (int i=0; i<32; ++i) {
        printf("%02x",digest[i]);
    }
    printf("\n");
    
    libftdb_c_ftdb_unload(ftdb_c);
    return EXIT_SUCCESS;
}
