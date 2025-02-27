#[cfg(feature = "postgres")]
mod postgres {
    use crate::{CsId, DerefId, FileId, FunctionId, GlobalId, LocalId, ModuleId, TypeId};
    use byteorder::{BigEndian, ByteOrder};
    use sqlx::postgres::{PgArgumentBuffer, PgHasArrayType, PgTypeInfo, PgValueFormat, PgValueRef};
    use sqlx::{encode::IsNull, error::BoxDynError, Decode, Encode, Postgres, Type};

    macro_rules! impl_traits {
        ($TypeName:ident) => {
            impl Type<Postgres> for $TypeName {
                fn type_info() -> PgTypeInfo {
                    PgTypeInfo::with_name("uint8")
                }
            }

            impl Encode<'_, Postgres> for $TypeName {
                fn encode_by_ref(&self, buf: &mut PgArgumentBuffer) -> Result<IsNull, BoxDynError> {
                    buf.extend(self.0.to_be_bytes());
                    Ok(IsNull::No)
                }
            }

            impl Decode<'_, Postgres> for $TypeName {
                fn decode(value: PgValueRef) -> Result<Self, BoxDynError> {
                    Ok(match value.format() {
                        PgValueFormat::Binary => BigEndian::read_u64(value.as_bytes()?).into(),
                        PgValueFormat::Text => value.as_str()?.parse::<u64>()?.into(),
                    })
                }
            }

            impl PgHasArrayType for $TypeName {
                fn array_type_info() -> PgTypeInfo {
                    PgTypeInfo::with_name("_uint8")
                }
            }
        };
    }

    impl_traits!(FunctionId);
    impl_traits!(GlobalId);
    impl_traits!(TypeId);
    impl_traits!(FileId);
    impl_traits!(ModuleId);
    impl_traits!(LocalId);
    impl_traits!(CsId);
    impl_traits!(DerefId);
}
