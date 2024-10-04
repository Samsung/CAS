use ahash::{HashMap, HashMapExt};
use ftdb::{Ftdb, FunctionId};
use std::collections::HashSet;

/// Represents a function in ExplicitCallgraph.
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct CallgraphEntry<'a> {
    /// The original FTDB id of the function.
    pub function_id: FunctionId,

    /// The split path to the source of the function.
    pub targetpath: Option<Vec<&'a str>>,
}

// TODO now - extend this with callgraph iterator traits

impl<'a> CallgraphEntry<'a> {
    /// Returns true if this CallgraphEntry is in the same 'module' as the
    /// other. For now this means that either the targetpath (reduced by ntail)
    /// of this CallgraphEntry is a prefix of the other's targetpath or
    /// otherwise. E.g. ["", "home", "you.c"] is in the same module unit as
    /// ["", "home", "me", "deep", "deep", "down.c"] for ntail = 1.
    ///
    /// Note: this is tentative, ntail should be replaced with a generic function
    /// comparison object that says if two functions belong to the same module.
    fn same_module_unit_as(&self, other: &CallgraphEntry<'a>, ntail: Option<u8>) -> bool {
        if ntail.is_none() {
            return true;
        }

        let tailv = ntail.unwrap() as usize;

        let my_targetpath = self
            .targetpath
            .as_ref()
            .expect("No targetpath, but required.");

        let my_targetpath_prefix_len = if tailv > my_targetpath.len() {
            0
        } else {
            my_targetpath.len() - tailv
        };

        let others_targetpath = other
            .targetpath
            .as_ref()
            .expect("No targetpath, but required.");

        let others_targetpath_prefix_len = if tailv > others_targetpath.len() {
            0
        } else {
            others_targetpath.len() - tailv
        };

        let my_prefix = &my_targetpath[..my_targetpath_prefix_len];
        let others_prefix = &others_targetpath[..others_targetpath_prefix_len];

        // check if my_prefix is contained in others_targetpath...
        if my_prefix.len() <= others_targetpath.len()
            && my_prefix
                .iter()
                .zip(others_targetpath.iter())
                .all(|(x, y)| x == y)
        {
            true
        } else {
            // ..or otherwise
            others_prefix
                .iter()
                .zip(my_targetpath.iter())
                .all(|(x, y)| x == y)
        }
    }
}

/// A representation of call graph and reverse call graph of a given FTDB.
/// Currently limited: contains only direct calls, no calls via function pointers.
#[derive(Clone, Debug)]
pub struct ExplicitCallgraph<'a> {
    /// A handle to FTDB.
    db: &'a Ftdb,

    /// A map from caller ids to callees' ids.
    pub id_graph: HashMap<CallgraphEntry<'a>, HashSet<CallgraphEntry<'a>>>,

    /// A map from callees ids to callers' ids.
    pub reverse_id_graph: HashMap<CallgraphEntry<'a>, HashSet<CallgraphEntry<'a>>>,
}

impl<'a> ExplicitCallgraph<'a> {
    /// Create call graph and reverse call graph from ftdb.
    pub fn new(ftdb: &'a Ftdb, mod_id_select: Option<u8>) -> ExplicitCallgraph<'a> {
        // compute call graph
        let id_graph: HashMap<CallgraphEntry, HashSet<CallgraphEntry>> = ftdb
            .funcs_iter()
            .map(|func| {
                let callees: HashSet<CallgraphEntry> = func
                    .calls()
                    .iter()
                    .map(|call| CallgraphEntry {
                        function_id: call.id,
                        targetpath: ExplicitCallgraph::get_split_source(
                            ftdb,
                            call.id,
                            mod_id_select,
                        ),
                    })
                    .collect();

                (
                    CallgraphEntry {
                        function_id: func.id(),
                        targetpath: ExplicitCallgraph::get_split_source(
                            ftdb,
                            func.id(),
                            mod_id_select,
                        ),
                    },
                    callees,
                )
            })
            .collect();

        // compute reverse call graph
        let mut reverse_id_graph: HashMap<CallgraphEntry, HashSet<CallgraphEntry>> = HashMap::new();
        id_graph.iter().for_each(|(caller, callees)| {
            callees.iter().for_each(|callee| {
                reverse_id_graph
                    .entry(callee.clone())
                    .or_default()
                    .insert(caller.clone());
            });
        });

        ExplicitCallgraph {
            db: ftdb,
            id_graph,
            reverse_id_graph,
        }
    }

    /// Compute and return all the callgraph entries that call functions from
    /// callgraph_entries (but only if they belong to the same module unit).
    fn preimage(
        &self,
        callgraph_entries: &HashSet<CallgraphEntry>,
        mod_id_select: Option<u8>,
    ) -> HashSet<CallgraphEntry> {
        let callees_of_funcs: HashSet<CallgraphEntry> = callgraph_entries
            .iter()
            .filter_map(|func| {
                // get all the entries that represent callers of func
                let optcallers = self.reverse_id_graph.get(func);

                // and leave only those that belong to the same module (if enabled)
                if let Some(callers) = optcallers.cloned() {
                    let callers_from_funcs_module: HashSet<CallgraphEntry> = callers
                        .into_iter()
                        .filter(|caller| func.same_module_unit_as(caller, mod_id_select))
                        .collect();
                    Some(callers_from_funcs_module)
                } else {
                    None
                }
            })
            .flatten()
            .collect();

        callees_of_funcs
    }

    /// Compute all the functions that call functions from func_id_set, either directly
    /// or indirectly, up to a given depth.
    ///
    /// For example: call with depth = 2 and func_id_set = {g} will return the set
    /// consisting of g, the callees of g, and the callees of callees of g.
    /// (Module restrictions omitted from this example.)
    ///
    /// The optional argument mod_id_select defines the part of the path that says that
    /// two functions belong to the same module unit. More precisely, two paths A and B
    /// point to the same module unit if either A with mod_id_select last path elements
    /// removed is a prefix of B or the other way around.
    ///
    /// # Examples
    ///
    /// ```
    /// # use std::collections::HashSet;
    /// # use ftdb::FunctionId;
    /// # use ftdb_ext::ExplicitCallgraph;
    /// # fn run(callgraph: &ExplicitCallgraph<'_>, InitFunIds: &HashSet<FunctionId>) {
    ///
    /// // assume that fun_1 from x/y/z/f.c is called by fun_2 from x/y/v/g.c and HashSet InitFunIds
    /// // contains the id of fun_1.
    ///
    /// // the result will contain fun_2; module-level restriction is off (None)
    /// let result = callgraph.compute_iterated_callees(InitFunIds, 1, None);
    ///
    /// // the result will not contain fun_2, because functions from x/y/z/f.c are in one module and functions from
    /// // x/y/v/g.c in other; module-level restriction 0 uses the entire path to identify modules
    /// let result = callgraph.compute_iterated_callees(InitFunIds, 1, Some(0));
    ///
    /// // the result will contain fun_2, because all the function from files nested under x/y/ are in the same module;
    /// // more precisely module-level restriction 2 checks that after cutting two last elements from the path to
    /// // fun_1's sources (i.e. x/y/z/f.c becomes x/y/) it is a prefix of the sources of fun_2 (or vice versa)
    /// let result = callgraph.compute_iterated_callees(InitFunIds, 1, Some(2));
    ///
    /// # }
    /// ```
    /// # Arguments
    ///
    /// * `func_id_set` - the initial set of callees
    /// * `depth` - the maximal length of call sequence
    /// * `mod_id_select` - the length of the path to the source that is considered when
    ///                     identifying modules
    ///
    pub fn compute_iterated_callees(
        &self,
        func_id_set: &HashSet<FunctionId>,
        mut depth: u8,
        mod_id_select: Option<u8>,
    ) -> HashSet<FunctionId> {
        let mut func_entries: HashSet<CallgraphEntry> = func_id_set
            .iter()
            .map(|function_id| CallgraphEntry {
                function_id: *function_id,
                targetpath: ExplicitCallgraph::get_split_source(
                    self.db,
                    *function_id,
                    mod_id_select,
                ),
            })
            .collect();

        let mut curr_func_entries_size = func_entries.len();

        while depth > 0 {
            func_entries.extend(self.preimage(&func_entries, mod_id_select));
            depth -= 1;

            // break if stabilized
            let updated_func_entries_size = func_entries.len();
            if updated_func_entries_size == curr_func_entries_size {
                break;
            }
            curr_func_entries_size = updated_func_entries_size;
        }

        func_entries
            .into_iter()
            .map(|entry| entry.function_id)
            .collect()
    }

    /// For a function with id function_id found in ftdb database, this function
    /// returns an array with path of source file split. For instance, call with
    /// function whose fid (in ftdb) points to "/home/tools/function.c" will
    /// return ["", "home", "tools", "function.c"].
    ///
    /// If mod_id_select is None then None is returned.
    ///
    fn get_split_source(
        ftdb: &'a Ftdb,
        function_id: FunctionId,
        mod_id_select: Option<u8>,
    ) -> Option<Vec<&'a str>> {
        mod_id_select?;

        // find fid, firstly looking for the function in funcs then in funcdecls
        let func_fid = match ftdb.func_by_id(function_id) {
            Some(func) => func.fid(),
            None => ftdb
                .funcdecl_by_id(function_id)
                .expect("Function id not found in funcs or funcdecls database!")
                .fid(),
        };

        let source_split: Vec<_> = ftdb
            .source_by_id(func_fid)
            .expect("No source for fid!")
            .split('/')
            .collect();

        Some(source_split)
    }
}
