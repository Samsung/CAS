pub mod explicit_callgraph;
mod iterators;

use self::iterators::CallgraphUpwardsIterator;
use ahash::{HashMap, HashMapExt, HashSet};
use ftdb::{Ftdb, FunctionEntry, FunctionId};
use iterators::CallgraphDownwardsIteratorBuilder;
use std::collections::VecDeque;

/// Provides operations on a callgraph
///
pub trait Callgraph {
    /// Using initial function set, iterate over it and all called functions.
    ///
    /// This is a recursive iteration limited by the depth parameter
    ///
    /// # Arguments
    ///
    /// * `initial` - Set of function IDs from which to traverse
    /// * `depths` - traversal depth limit, depth 0 means initial set
    /// * `iterate_initial` - if true initial set will be included in iteration
    ///
    fn iter_down<T>(&self, initial: T) -> CallgraphDownwardsIteratorBuilder<'_>
    where
        T: Iterator<Item = FunctionId>;

    /// Using initial function set, iterate over it and all callees of these
    /// functions.
    ///
    /// This is a recursive iteration limited by the depth parameter.
    ///
    fn iter_up<T>(
        &self,
        initial: T,
        depth: u8,
    ) -> impl Iterator<Item = (FunctionId, FunctionEntry<'_>, u8)>
    where
        T: IntoIterator<Item = FunctionId>;
}

impl Callgraph for Ftdb {
    fn iter_down<T>(&self, initial: T) -> CallgraphDownwardsIteratorBuilder<'_>
    where
        T: Iterator<Item = FunctionId>,
    {
        CallgraphDownwardsIteratorBuilder::new(self, initial)
    }

    fn iter_up<T>(
        &self,
        initial: T,
        depth: u8,
    ) -> impl Iterator<Item = (FunctionId, FunctionEntry<'_>, u8)>
    where
        T: IntoIterator<Item = FunctionId>,
    {
        let mut reverse_calls: HashMap<_, HashSet<_>> = HashMap::new();
        for caller in self.funcs_iter() {
            for callee_id in caller.call_ids() {
                reverse_calls
                    .entry(*callee_id)
                    .or_default()
                    .insert(caller.id());
            }
        }

        // Collecting to HashSet to ensure uniqueness of the initial set
        //
        let initial = initial.into_iter().collect::<HashSet<_>>();

        let queue: VecDeque<_> = initial
            .iter()
            .filter_map(|id| self.func_by_id(*id))
            .filter_map(|callee| {
                reverse_calls
                    .get(&callee.id())
                    .map(|callers| (callee.id(), callers))
            })
            .flat_map(|(callee_id, callers)| {
                callers.iter().map(move |caller_id| (callee_id, caller_id))
            })
            // Some entries might be funcdecls which are not really interesting
            .filter_map(|(callee_id, caller_id)| {
                self.func_by_id(*caller_id)
                    .map(|caller| (callee_id, caller))
            })
            .collect();

        CallgraphUpwardsIterator::new(self, reverse_calls, queue, initial, depth)
    }
}
