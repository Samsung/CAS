use std::{
    env, fs,
    path::{Path, PathBuf},
    process::exit,
};

fn main() {
    let out_dir = env::var("OUT_DIR").map(PathBuf::from).unwrap();
    bindgen_ftdb(&out_dir);
    build_native_lib(&out_dir);

    println!("cargo:rustc-link-lib=dylib=ftdb_c");
    println!("cargo:rustc-link-lib=dylib=stdc++");
}

fn build_native_lib(out_dir: &Path) {
    // Do not build the shared library if build comes from the docs.rs tools
    if env::var("DOCS_RS").is_ok() {
        return;
    }

    // When user provides FTDB_LIB_DIR env, do not compile any libraries
    if let Some(build_dir) = option_env!("FTDB_LIB_DIR").map(PathBuf::from) {
        if !build_dir.join("libftdb_c.so").exists() {
            eprintln!(
                "FTDB_LIB_DIR {} does not contain libftdb_c.so file. Make sure to compile the ftdb_c target!",
                build_dir.display()
            );
            exit(1);
        }
        println!("cargo:rustc-link-search=native={}", build_dir.display());

        // Let's copy the lib so `cargo run` works properly
        let src = build_dir.join("libftdb_c.so");
        let dst = out_dir.join("libftdb_c.so");
        create_hardlink(&src, &dst);
        println!("cargo:rustc-link-search=native={}", out_dir.display());
        return;
    }
    // When building from CAS repository, the `bindings/cas` points to the
    // root CAS project directory
    let root_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    if let Ok(cas_dir) = root_dir.join("bindings/cas").canonicalize() {
        let cmake_dir = cmake::Config::new(cas_dir).build_target("ftdb_c").build();
        println!(
            "cargo:rustc-link-search=native={}",
            cmake_dir.join("build/ftdb").display()
        );
    }
}

fn bindgen_ftdb(out_dir: &Path) {
    let ftdb_include_dir = option_env!("FTDB_INCLUDE_DIR")
        .map(PathBuf::from)
        .or_else(|| Some(PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("bindings/includes")))
        .unwrap();
    let bindings = bindgen::Builder::default()
        .clang_arg(format!("-I/{}/ftdb", ftdb_include_dir.display()))
        .clang_arg(format!(
            "-I/{}/kflat/lib/include_priv",
            ftdb_include_dir.display()
        ))
        .header("bindings/bindings.h")
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

    bindings
        .write_to_file(out_dir.join("ftdb.rs"))
        .expect("Unable to write bindings");
}

fn create_hardlink(src: &Path, dst: &Path) {
    if let Err(err) = fs::hard_link(src, dst) {
        if let Err(copy_err) = fs::copy(src, dst) {
            eprintln!(
                "Could not create hardlink nor copy file: {} -> {}",
                src.display(),
                dst.display()
            );
            eprintln!("hardlink error: {err}");
            eprintln!("copy error: {copy_err}");
            exit(1);
        }
    }
}
