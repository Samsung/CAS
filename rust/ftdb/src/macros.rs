macro_rules! impl_inner_handle {
    ($struct_name:ident) => {
        impl Handle for $struct_name {
            fn handle(&self) -> std::sync::Arc<FtdbHandle> {
                self.0.handle()
            }
        }
    };
}

macro_rules! impl_identifier {
    ($struct_name:ident) => {
        #[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone, Copy, Hash)]
        #[repr(transparent)]
        pub struct $struct_name(pub u64);

        impl std::fmt::Display for $struct_name {
            fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
                write!(f, "{}", self.0)
            }
        }

        impl From<u64> for $struct_name {
            fn from(value: u64) -> Self {
                $struct_name(value)
            }
        }

        impl From<&u64> for $struct_name {
            fn from(value: &u64) -> Self {
                $struct_name(*value)
            }
        }

        impl From<usize> for $struct_name {
            fn from(value: usize) -> Self {
                $struct_name(value.try_into().expect("usize must convert to u64"))
            }
        }

        impl From<&usize> for $struct_name {
            fn from(value: &usize) -> Self {
                $struct_name((*value).try_into().expect("usize must convert to u64"))
            }
        }

        impl TryFrom<i64> for $struct_name {
            type Error = &'static str;

            fn try_from(value: i64) -> Result<Self, Self::Error> {
                if value >= 0 {
                    Ok($struct_name(value as u64))
                } else {
                    Err("identifier not expected to be < 0")
                }
            }
        }

        impl TryFrom<&i64> for $struct_name {
            type Error = &'static str;

            fn try_from(value: &i64) -> Result<Self, Self::Error> {
                if *value >= 0 {
                    Ok($struct_name(*value as u64))
                } else {
                    Err("identifier not expected to be < 0")
                }
            }
        }

        impl From<$struct_name> for usize {
            fn from(value: $struct_name) -> Self {
                value.0.try_into().unwrap()
            }
        }

        impl From<$struct_name> for u64 {
            fn from(value: $struct_name) -> Self {
                value.0
            }
        }

        #[cfg(feature = "serde")]
        impl serde::Serialize for $struct_name {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: serde::Serializer,
            {
                self.0.serialize(serializer)
            }
        }

        #[cfg(feature = "serde")]
        impl<'de> serde::Deserialize<'de> for $struct_name {
            fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
            where
                D: serde::Deserializer<'de>,
            {
                let inner = u64::deserialize(deserializer)?;
                Ok($struct_name::from(inner))
            }
        }
    };
}

macro_rules! impl_expr_iface {
    ($struct_name:ident) => {
        impl<'a> $struct_name<'a> {
            /// Returns what kind of expression it is (local variable,
            /// global or maybe a string literal)
            ///
            pub fn expr_type(&self) -> ExprType {
                self.0.type_.into()
            }

            /// Returns additional data about the expression, depending on its type
            /// (id of a local, id of a global, integer literal's value, etc)
            ///
            pub fn expr_data(&self) -> $crate::ExprData {
                match self.expr_type() {
                    ExprType::Function => {
                        let func_id = $crate::utils::try_ptr_to_type(self.as_inner_ref().id)
                            .expect("'id' field must be set for callref type 'function'")
                            .into();
                        $crate::ExprData::FunctionPtrVar(func_id)
                    }
                    ExprType::Global => {
                        let global_id = $crate::utils::try_ptr_to_type(self.as_inner_ref().id)
                            .expect("'id' field must be set for callref type 'global'")
                            .into();
                        $crate::ExprData::GlobalVar(global_id)
                    }
                    ExprType::Local | ExprType::LocalParm => {
                        let local_id = $crate::utils::try_ptr_to_type(self.as_inner_ref().id)
                            .expect("'id' field must be set for callref type 'local' or 'parm'")
                            .into();
                        $crate::ExprData::LocalVar(local_id)
                    }
                    ExprType::StringLiteral => {
                        let value =
                            $crate::utils::try_ptr_to_str(self.as_inner_ref().string_literal)
                                .expect(
                                "'string_literal' field must be set for callref 'string_literal'",
                            );
                        $crate::ExprData::StringLiteral(value)
                    }
                    ExprType::CharLiteral => {
                        let value = $crate::utils::try_ptr_to_type(
                            self.as_inner_ref().character_literal,
                        )
                        .expect(
                            "'character_literal' field must be set for callref 'character_literal'",
                        );
                        $crate::ExprData::CharacterLiteral(value)
                    }
                    ExprType::IntegerLiteral => {
                        let value =
                            $crate::utils::try_ptr_to_type(self.as_inner_ref().integer_literal)
                                .expect(
                                "'integer_literal' field must be set for callref 'integer_literal'",
                            );
                        $crate::ExprData::IntegerLiteral(value)
                    }
                    ExprType::FloatLiteral => {
                        let value =
                            $crate::utils::try_ptr_to_type(self.as_inner_ref().floating_literal)
                                .expect(
                                    "'floating_literal' field must be set for callref 'floating'",
                                );
                        $crate::ExprData::FloatingLiteral(value)
                    }
                    _ => {
                        if cfg!(debug_assertions) {
                            unimplemented!()
                        } else {
                            $crate::ExprData::None
                        }
                    }
                }
            }
        }
    };
}

pub(crate) use impl_expr_iface;
pub(crate) use impl_identifier;
pub(crate) use impl_inner_handle;
