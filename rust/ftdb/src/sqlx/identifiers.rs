use ::sqlx::{encode::IsNull, error::BoxDynError, Decode, Encode, Postgres, Type};
use byteorder::{BigEndian, ByteOrder};

#[cfg(feature = "postgres")]
mod postgres {
    use super::*;
    use crate::{FunctionId, GlobalId, TypeId};
    use sqlx::postgres::{PgArgumentBuffer, PgHasArrayType, PgTypeInfo, PgValueFormat, PgValueRef};

    macro_rules! impl_traits {
        ($TypeName:ident) => {
            impl Type<Postgres> for $TypeName {
                fn type_info() -> PgTypeInfo {
                    PgTypeInfo::with_name("uint8")
                }
            }

            impl Encode<'_, Postgres> for $TypeName {
                fn encode_by_ref(&self, buf: &mut PgArgumentBuffer) -> IsNull {
                    buf.extend(self.0.to_be_bytes());
                    IsNull::No
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
}
