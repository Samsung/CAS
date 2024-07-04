macro_rules! impl_inner_display {
    ($struct_name:ident) => {
        impl std::fmt::Display for $struct_name {
            fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
                write!(f, "{}", self.0)
            }
        }
    };
}

macro_rules! impl_inner_handle {
    ($struct_name:ident) => {
        impl Handle for $struct_name {
            fn handle(&self) -> std::sync::Arc<FtdbHandle> {
                self.0.handle()
            }
        }
    };
}

pub(crate) use impl_inner_display;
pub(crate) use impl_inner_handle;
