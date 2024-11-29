#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <unflatten.hpp>
#include "ftdb.h"

struct ftdb_c {
    bool init_done;
    int verbose;
    int debug;
    const struct ftdb* ftdb;
    CUnflatten unflatten;
};

#define FTDB_C_TYPE(ftdb_c) ((struct ftdb_c*)ftdb_c)

CFtdb libftdb_c_ftdb_load(const char* filename, int quiet, int debug) {

    bool err = true;

    struct ftdb_c* ftdb_c = malloc(sizeof(struct ftdb_c));
    ftdb_c->verbose = !quiet;
    ftdb_c->debug = debug;

    FILE* in = fopen(filename, "r+b");
    if(!in) {
        in = fopen(filename, "rb");
        if(!in) {
            fprintf(stderr,"Cannot open cache file - (%d) %s\n", errno, strerror(errno));
            goto done;
        }
    }

    int debug_level = 0;
    if(debug) debug_level = 2;
    else if(!quiet) debug_level = 1;

    ftdb_c->unflatten = unflatten_init(debug_level);
    if(ftdb_c->unflatten == NULL) {
        fprintf(stderr,"Failed to intialize unflatten library\n");
        goto done;
    }

    UnflattenStatus status = unflatten_load_continuous(ftdb_c->unflatten, in, NULL);
    if (status) {
        fprintf(stderr,"Failed to read cache file: %s\n", unflatten_explain_status(status));
        unflatten_deinit(ftdb_c->unflatten);
        goto done;
    }
    fclose(in);

    ftdb_c->ftdb = (const struct ftdb*) unflatten_root_pointer_next(ftdb_c->unflatten);

    /* Check whether it's correct file and in supported version */
    if(ftdb_c->ftdb->db_magic != FTDB_MAGIC_NUMBER) {
        fprintf(stderr,"Failed to parse cache file - invalid magic %llu\n", ftdb_c->ftdb->db_magic);
        unflatten_deinit(ftdb_c->unflatten);
        goto done;
    }
    if(ftdb_c->ftdb->db_version != FTDB_VERSION) {
        fprintf(stderr,"Failed to parse cache file - unsupported image version %llu (required: %llu)\n", ftdb_c->ftdb->db_version, FTDB_VERSION);
        unflatten_deinit(ftdb_c->unflatten);
        goto done;
    }

    ftdb_c->init_done = true;
    err = false;

done:
    if (err) {
        free(ftdb_c);
        return NULL;	/* Indicate that exception has been set */
    }

    return ftdb_c;
}

void libftdb_c_ftdb_unload(CFtdb ftdb_c) {
    unflatten_deinit(FTDB_C_TYPE(ftdb_c)->unflatten);
    free(ftdb_c);
}

struct ftdb* libftdb_c_ftdb_object(CFtdb ftdb_c) {
    return (struct ftdb*)FTDB_C_TYPE(ftdb_c)->ftdb;
}

struct ftdb_type_entry* libftdb_c_get_type_entry_by_id(CFtdb ftdb_c, unsigned long id) {

    struct ulong_entryMap_node* node = ulong_entryMap_search(&FTDB_C_TYPE(ftdb_c)->ftdb->refmap, id);
    if (node)
        return (struct ftdb_type_entry*)node->entry;
    return 0;
}
