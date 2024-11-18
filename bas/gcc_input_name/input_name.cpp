#include <stdio.h>
#include <stdlib.h>

/*
 * Parsing gcc command line in the search for the input file(s) passed to the compiler is not a trivial task in general
 * So here we have an extraordinarily complicated gcc plugin that can actually extract the name(s) for us from gcc internals
 */

/* All plugin sources should start including "gcc-plugin.h". */
#include "gcc-plugin.h"

/* This let us inspect the GENERIC intermediate representation. */
#include "tree.h"

/* All plugins must export this symbol so that they can be linked with GCC license-wise. */
int plugin_is_GPL_compatible;

extern const char **in_fnames;
extern unsigned num_in_fnames;

void fn_start_unit_print_compiled_filename(void *gcc_data, void *user_data) {
    
    unsigned i;
    for (i=0; i<num_in_fnames; ++i) {
        printf("%s\n",in_fnames[i]);
    }
    exit(0);
   
}

/* Most interesting part so far: this is the plugin entry point. */
int plugin_init (struct plugin_name_args *plugin_info, struct plugin_gcc_version *version) {

    (void) version;
    /* Give GCC a proper name and version number for this plugin. */
    const char *plugin_name = plugin_info->base_name;
    struct plugin_info pi = { "0.1", "Plugin to get names of files under compilation" };
    register_callback (plugin_name, PLUGIN_START_UNIT, fn_start_unit_print_compiled_filename, &pi);

    /* Check everything is fine displaying a familiar message. */
 
    return 0;
}
