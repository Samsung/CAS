use super::{borrowed, Functions};
use crate::{db::collection::BorrowedIterator, FunctionId};
use ftdb_sys::ftdb::ftdb_func_entry;
use std::collections::HashSet;

/// Trait extension to provide depth-first search access to a list
/// of function trees.
///
pub trait DfsFunctionExt<'f> {
    /// Get DFS iterator of FunctionEntry objects
    ///
    fn iter_dfs(&'f self) -> BorrowedDfsFunctionIterator<'f>;
}

impl<'f> DfsFunctionExt<'f> for Functions {
    fn iter_dfs(&'f self) -> BorrowedDfsFunctionIterator<'f> {
        BorrowedDfsFunctionIterator {
            db: self,
            visited: HashSet::default(),
            stack: Vec::default(),
            iter: self.iter(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct BorrowedDfsFunctionIterator<'f> {
    db: &'f Functions,
    visited: HashSet<FunctionId>,
    stack: Vec<borrowed::FunctionEntry<'f>>,
    iter: BorrowedIterator<'f, Functions, borrowed::FunctionEntry<'f>, ftdb_func_entry>,
}

impl<'f> BorrowedDfsFunctionIterator<'f> {
    fn visit_inner(
        &mut self,
        func: borrowed::FunctionEntry<'f>,
    ) -> Option<borrowed::FunctionEntry<'f>> {
        self.visited.insert(func.id());

        let new_calls = func
            .call_ids()
            .iter()
            .copied()
            .filter_map(|fid| {
                if !self.visited.contains(&fid) {
                    self.db.entry_by_id(fid)
                } else {
                    None
                }
            })
            .collect::<Vec<_>>();

        // Every callee has already been visited
        if new_calls.is_empty() {
            Some(func)
        } else {
            self.stack.push(func);
            self.stack.extend(new_calls);
            None
        }
    }
}

impl<'f> Iterator for BorrowedDfsFunctionIterator<'f> {
    type Item = borrowed::FunctionEntry<'f>;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if let Some(func) = self.stack.pop() {
                match self.visit_inner(func) {
                    Some(f) => return Some(f),
                    None => continue,
                }
            }
            if let Some(func) = self.iter.next() {
                match self.visit_inner(func) {
                    Some(f) => return Some(f),
                    None => continue,
                }
            }
            return None;
        }
    }
}
