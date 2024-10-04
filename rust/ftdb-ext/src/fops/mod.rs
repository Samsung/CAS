use ftdb::{FopsEntry, Ftdb, FunctionEntry, FunctionId};
use specs::config::{Specs as ConfigSpecs, BUILTIN_VMLINUX_SPECS};
use specs::{database::Specs, MemberSpec};
use std::collections::VecDeque;

use crate::callgraph::Callgraph;

pub mod specs;

#[derive(Debug, thiserror::Error)]
pub enum ProcessError {
    #[error("Specs config file could not be read")]
    SpecsReadError(#[from] serde_json::Error),
}

pub fn process_vmlinux_fops(db: &Ftdb) -> Result<FopsCategoryIterator<'_>, ProcessError> {
    let config = BUILTIN_VMLINUX_SPECS;
    let config: ConfigSpecs = serde_json::from_str(config)?;
    let specs = Specs::new(db, config);
    let fops_iter = Box::new(db.fops_iter());
    Ok(FopsCategoryIterator {
        specs,
        items: VecDeque::new(),
        fops_iter,
    })
}

pub fn funcs_reachable_from_vmlinux_fopses(
    db: &Ftdb,
    depth: u8,
) -> impl Iterator<Item = FunctionEntry<'_>> {
    let initial = process_vmlinux_fops(db)
        .expect("Builtin specs file must be valid")
        .map(|entry| entry.func_id);
    db.iter_down(initial)
        .with_depth(depth)
        .include_initial()
        .into_iter()
        .map(|(_caller_id, func, _depth)| func)
}

pub struct FopsCategoryIterator<'a> {
    specs: Specs,
    items: VecDeque<(FunctionId, MemberSpec)>,
    fops_iter: Box<dyn Iterator<Item = FopsEntry<'a>> + 'a>,
}

pub struct FopsCategory {
    pub func_id: FunctionId,
    pub category: String,
    pub user: Vec<i64>,
}

impl From<(FunctionId, MemberSpec)> for FopsCategory {
    fn from(value: (FunctionId, MemberSpec)) -> Self {
        Self {
            func_id: value.0,
            category: value.1.category,
            user: value.1.user,
        }
    }
}

impl<'a> Iterator for FopsCategoryIterator<'a> {
    type Item = FopsCategory;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if let Some(item) = self.items.pop_front() {
                return Some(FopsCategory::from(item));
            }
            if let Some(entry) = self.fops_iter.next() {
                let type_id = entry.type_id();
                let members_iter = entry
                    .members_iter()
                    .map(|fme| {
                        let member_idx = fme.member_id();
                        let func_ids = fme.func_ids().to_vec();
                        let specs = self.specs.get(type_id, member_idx);
                        (specs, func_ids)
                    })
                    .filter(|(specs, func_ids)| specs.is_some() && !func_ids.is_empty())
                    .flat_map(|(member_spec, func_ids)| {
                        assert!(member_spec.is_some());
                        let member_spec = unsafe { member_spec.unwrap_unchecked() };
                        func_ids
                            .into_iter()
                            .map(|func_id| (func_id, member_spec.clone()))
                    });
                self.items.extend(members_iter);
            } else {
                return None;
            }
        }
    }
}
