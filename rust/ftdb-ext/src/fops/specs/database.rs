use ahash::HashMap;
use ftdb::{Ftdb, TypeClass, TypeId};

#[derive(Debug)]
pub struct Specs(
    // (TypeId, member index) -> category name
    HashMap<(TypeId, usize), MemberSpec>,
);

#[derive(Debug, Clone)]
pub struct MemberSpec {
    pub category: String,
    pub user: Vec<i64>,
}

impl Specs {
    pub fn new(db: &Ftdb, config: super::config::Specs) -> Self {
        let inner = config
            .0
            .iter()
            .flat_map(|struct_spec| {
                // Find out all types matching given name
                db.types_by_name(&struct_spec.name)
                    .filter(|t| t.classid() == TypeClass::Record)
                    .flat_map(|t| {
                        struct_spec.members.iter().flat_map(move |member_spec| {
                            // Find out what is the index of a member of specified name
                            let indices = if member_spec.name == "*" {
                                t.refnames_iter()
                                    .enumerate()
                                    .map(|(i, _)| i)
                                    .collect::<Vec<_>>()
                            } else {
                                // Find a member of a given name
                                t.refnames_iter()
                                    .position(|refname| refname == member_spec.name)
                                    .into_iter()
                                    .collect::<Vec<_>>()
                            };
                            let type_id = t.id();
                            let category = member_spec.category.to_string();
                            let user = member_spec.user.as_ref().cloned().unwrap_or_default();
                            indices.into_iter().map(move |member_index| {
                                let key = (type_id, member_index);
                                let value = MemberSpec {
                                    category: category.clone(),
                                    user: user.clone(),
                                };
                                (key, value)
                            })
                        })
                    })
            })
            .collect::<HashMap<(TypeId, usize), MemberSpec>>();

        Self(inner)
    }

    pub fn get(&self, type_id: TypeId, member_idx: usize) -> Option<&MemberSpec> {
        self.0.get(&(type_id, member_idx))
    }
}
