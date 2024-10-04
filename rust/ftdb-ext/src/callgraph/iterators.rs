use ahash::{HashMap, HashSet};
use ftdb::{Ftdb, FunctionEntry, FunctionId};
use std::collections::VecDeque;

pub struct CallgraphDownwardsIteratorBuilder<'a> {
    db: &'a Ftdb,
    initial: HashSet<FunctionId>,
    cut_funcs: HashSet<FunctionId>,
    depth: u8,
    iterate_initial: bool,
}

impl<'a> CallgraphDownwardsIteratorBuilder<'a> {
    /// Create a new builder instance to select/setup iterator
    ///
    /// # Arguments
    ///
    /// * `db` - database source
    /// * `initial` - set of function IDs
    ///
    pub(crate) fn new<T>(db: &'a Ftdb, initial: T) -> Self
    where
        T: Iterator<Item = FunctionId>,
    {
        Self {
            db,
            initial: initial.collect::<HashSet<_>>(),
            cut_funcs: Default::default(),
            depth: u8::MAX,
            iterate_initial: false,
        }
    }

    pub fn include_initial(mut self) -> Self {
        self.iterate_initial = true;
        self
    }

    /// Provide set of functions on which to cut graph traversal
    ///
    pub fn with_cut_funcs<T>(mut self, funcs: T) -> Self
    where
        T: Iterator<Item = FunctionId>,
    {
        self.cut_funcs.extend(funcs);
        self
    }

    /// Limit depth of a graph traversal
    ///
    pub fn with_depth(mut self, depth: u8) -> Self {
        self.depth = depth;
        self
    }

    /// Create an instance of actual iterator
    ///
    pub fn into_iter(self) -> CallgraphDownwardsIterator<'a> {
        CallgraphDownwardsIterator::new(
            self.db,
            self.initial,
            self.depth,
            self.iterate_initial,
            self.cut_funcs,
        )
    }
}

pub struct CallgraphDownwardsIterator<'a> {
    /// Handle to database
    db: &'a Ftdb,

    /// Batch of functions being currently processed (current depth)
    queue_current: VecDeque<(FunctionId, FunctionEntry<'a>)>,

    /// Next batch of functions to process (next depth)
    queue_next: VecDeque<(FunctionId, FunctionEntry<'a>)>,

    /// Mark functions as visisted
    visited: HashSet<FunctionId>,

    /// Current BFS depth
    cur_depth: u8,

    /// BFS depth limit
    max_depth: u8,

    /// Flag to enable iteration on initial set
    iterate_initial: bool,

    /// Set of functions at which to cut traversal
    cut_funcs: HashSet<FunctionId>,
}

impl<'a> CallgraphDownwardsIterator<'a> {
    /// Construct a new downwards graph iterator
    ///
    /// # Arguments
    ///
    /// * `initial` - Set of function IDs from which to traverse
    /// * `depths` - traversal depth limit, depth 0 means initial set
    /// * `iterate_initial` - if true initial set will be included in iteration
    ///
    pub(crate) fn new(
        db: &'a Ftdb,
        initial: HashSet<FunctionId>,
        depth: u8,
        iterate_initial: bool,
        cut_funcs: HashSet<FunctionId>,
    ) -> Self {
        let queue = initial
            .iter()
            .filter(|func_id| !cut_funcs.contains(func_id))
            .filter_map(|func_id| db.func_by_id(*func_id))
            .map(|func| (FunctionId::none(), func))
            .collect::<VecDeque<_>>();
        Self {
            db,
            queue_current: queue,
            queue_next: VecDeque::new(),
            visited: initial,
            cur_depth: 0,
            max_depth: depth,
            iterate_initial,
            cut_funcs,
        }
    }
}

impl<'a> Iterator for CallgraphDownwardsIterator<'a> {
    type Item = (FunctionId, FunctionEntry<'a>, u8);

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            // Process current batch
            if let Some((parent_id, func)) = self.queue_current.pop_front() {
                for child_id in func.call_ids() {
                    if self.visited.insert(*child_id) && !self.cut_funcs.contains(child_id) {
                        if let Some(child_func) = self.db.func_by_id(*child_id) {
                            self.queue_next.push_back((func.id(), child_func));
                        }
                    }
                }
                // Skip initial set if flag is enabled
                if !self.iterate_initial && parent_id.is_none() {
                    continue;
                }
                return Some((parent_id, func, self.cur_depth));
            }

            // Current batch processing is finished. Let's do some iteration
            // stop conditions and move to the next batch.

            // We reached traversal depth limit
            if self.cur_depth == self.max_depth {
                return None;
            }
            // No more functions to iterate
            if self.queue_next.is_empty() {
                return None;
            }
            // Go to the next batch (depth)
            std::mem::swap(&mut self.queue_current, &mut self.queue_next);
            self.cur_depth += 1;
        }
    }
}

pub struct CallgraphUpwardsIterator<'a> {
    db: &'a Ftdb,
    reverse_calls: HashMap<FunctionId, HashSet<FunctionId>>,
    queue_current: VecDeque<(FunctionId, FunctionEntry<'a>)>,
    queue_next: VecDeque<(FunctionId, FunctionEntry<'a>)>,
    visited: HashSet<FunctionId>,
    depth: u8,
}

impl<'a> CallgraphUpwardsIterator<'a> {
    pub(crate) fn new(
        db: &'a Ftdb,
        reverse_calls: HashMap<FunctionId, HashSet<FunctionId>>,
        initial_queue: VecDeque<(FunctionId, FunctionEntry<'a>)>,
        visited: HashSet<FunctionId>,
        depth: u8,
    ) -> Self {
        Self {
            db,
            reverse_calls,
            queue_current: initial_queue,
            queue_next: VecDeque::new(),
            visited,
            depth,
        }
    }
}

impl<'a> Iterator for CallgraphUpwardsIterator<'a> {
    type Item = (FunctionId, FunctionEntry<'a>, u8);

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if let Some((callee_id, caller_func)) = self.queue_current.pop_front() {
                if let Some(new_callee_candidates) = self.reverse_calls.get(&caller_func.id()) {
                    for new_callee_id in new_callee_candidates {
                        // We ignored funcdecls at the reverse_call construction time
                        let func = self
                            .db
                            .func_by_id(*new_callee_id)
                            .expect("funcdecls must be filter out");

                        if self.visited.insert(*new_callee_id) {
                            self.queue_next.push_back((caller_func.id(), func));
                        }
                    }
                }
                return Some((callee_id, caller_func, self.depth));
            }
            if self.depth == 0 {
                return None;
            }
            if self.queue_next.is_empty() {
                return None;
            }
            self.queue_current.extend(self.queue_next.iter().cloned());
            self.queue_next.clear();
            self.depth -= 1;
        }
    }
}
