use ftdb::{Ftdb, FunctionEntry, FunctionId, GlobalEntry, GlobalId};

/// Single debugfs entry
///
/// The main part is `function_id` field which refers to actual function
/// being marked as debugfs. Other fields are just some useful metadata.
///
#[derive(Debug, serde::Serialize)]
pub struct DebugfsEntry {
    /// Id of a function marked as debugfs
    pub function_id: FunctionId,

    /// Id of a global that holds pointer to a function ref'd by `function_id`
    pub global_id: GlobalId,

    /// Id of a function in which debugfs registration happens
    pub registrar_id: FunctionId,
}

/// Structure to hold state of iteration over debugfs functions.
///
pub struct DebugfsIterator<'a> {
    db: &'a Ftdb,
    func_iter: Box<dyn ExactSizeIterator<Item = FunctionEntry<'a>> + 'a>,
    current_func: Option<FunctionEntry<'a>>,
    current_global: Option<GlobalEntry<'a>>,
    debugfs_ids: Vec<FunctionId>,
    globals: Vec<GlobalId>,
    funrefs: Vec<FunctionId>,
}

impl<'a> DebugfsIterator<'a> {
    pub fn new(db: &'a Ftdb) -> DebugfsIterator<'a> {
        let debugfs_names = &[
            "debugfs_create_file",
            "debugfs_create_file_unsafe",
            "debugfs_create_file_size",
        ];
        let debugfs_ids: Vec<_> = debugfs_names
            .iter()
            .flat_map(|func_name| db.funcs_by_name(func_name).map(|f| f.id()))
            .collect();
        DebugfsIterator {
            db,
            func_iter: Box::new(db.funcs_iter()),
            debugfs_ids,
            globals: Vec::new(),
            current_func: None,
            current_global: None,
            funrefs: Vec::new(),
        }
    }
}

impl<'a> Iterator for DebugfsIterator<'a> {
    type Item = DebugfsEntry;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if let Some(function_id) = self.funrefs.pop() {
                let global_id = self
                    .current_global
                    .as_ref()
                    .expect("Current global must be set")
                    .id();
                let registrar_id = self
                    .current_func
                    .as_ref()
                    .expect("Current function must be set")
                    .id();

                return Some(DebugfsEntry {
                    function_id,
                    global_id,
                    registrar_id,
                });
            }
            // Get next global from filtered global lists of current function
            if let Some(global_id) = self.globals.pop() {
                let global = self
                    .db
                    .global_by_id(global_id)
                    .expect("Database corrupted: global must exists!");
                self.funrefs.extend(global.funrefs());
                self.current_global.replace(global);
                continue;
            }
            // No more globals, lets get new global list from the next function

            self.current_func = self.func_iter.next();
            if let Some(current_func) = self.current_func.as_ref() {
                let globals = current_func
                    .calls()
                    .into_iter()
                    .filter(|ce| self.debugfs_ids.contains(&ce.id))
                    .flat_map(|ce| {
                        ce.data_iter()
                            .filter_map(|rfi| match (rfi.pos(), rfi.expr_data()) {
                                (4, ftdb::ExprData::GlobalVar(global_id)) => Some(global_id),
                                _ => None,
                            })
                    });

                self.globals.extend(globals);
                continue;
            }

            // No more data - stop iterator!
            return None;
        }
    }
}

/// Iterate over functions marked as debugfs
///
/// Function is considered debugfs when it is passed as 4th argument in a
/// structure to one of the following functions
///
/// - `debugfs_create_file`
/// - `debugfs_create_file_unsafe`
/// - `debugfs_create_file_size`
///
/// Note that in this implementation we consider only global variables as these
/// structures passed to registration functions.
///
pub fn process_vmlinux_debugfs(db: &Ftdb) -> DebugfsIterator<'_> {
    DebugfsIterator::new(db)
}
