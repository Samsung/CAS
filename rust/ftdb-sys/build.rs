use std::env;
use std::path::PathBuf;

fn main() {
    let ftdb_include_dir = env::var("FTDB_INCLUDE_DIR").expect("FTDB_INCLUDE_DIR not set");
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    // bindgen_kflat(&ftdb_include_dir, &out_path);
    bindgen_ftdb(&ftdb_include_dir, &out_path);
    if let Ok(lib_path) = env::var("FTDB_LIB_DIR") {
        println!("cargo:rustc-link-search=native={lib_path:}");
    }
    println!("cargo:rustc-link-lib=dylib=ftdb_c");
    println!("cargo:rustc-link-lib=dylib=stdc++");
    println!("cargo:rerun-if-changed=ftdb.rs");
}

fn bindgen_ftdb(ftdb_include_dir: &str, out_path: &PathBuf) {
    let bindings = bindgen::Builder::default()
        .clang_arg(format!("-I/{}/bas", ftdb_include_dir))
        .clang_arg(format!("-I/{}/kflat/lib/include_priv", ftdb_include_dir))
        .header("bindings_ftdb.h")
        .layout_tests(false)
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .use_core()
        .ctypes_prefix("::libc")
        .size_t_is_usize(true)
        .allowlist_type(".*_data")
        .allowlist_type(".*_entry")
        .allowlist_type(".*_info")
        .allowlist_type(".*_item")
        .allowlist_type(".*_node")
        .allowlist_type(".*Type")
        .allowlist_type("bitfield")
        .allowlist_type("csitem")
        .allowlist_type("fops.*")
        .allowlist_type("ftdb.*")
        .allowlist_type("function.*")
        .allowlist_type("rb_.*")
        .allowlist_type("refcall")
        .allowlist_type("stringRef.*")
        .allowlist_type("taint.*")
        .allowlist_type("Type.*")
        .allowlist_type("CFtdb")
        .blocklist_type("nfsdb_.*")
        .allowlist_function("libftdb_.+")
        .allowlist_function("stringRef_.+")
        .allowlist_function("ulong_.+")
        .allowlist_var("FTDB_MAGIC_NUMBER")
        .allowlist_var("FTDB_VERSION")
        .generate_inline_functions(true)
        .allowlist_recursively(false)
        .default_enum_style(bindgen::EnumVariation::Rust {
            non_exhaustive: true,
        })
        .constified_enum("globalDefType")
        .translate_enum_integer_types(true)
        .generate()
        .expect("Unable to generate ftdb bindings");

    store(bindings, out_path, "ftdb.rs");
}

fn store(bindings: bindgen::Bindings, out_path: &PathBuf, path: &str) {
    bindings
        .write_to_file(out_path.join(path))
        .expect(format!("Unable to write {} bindings", path).as_str());
}
