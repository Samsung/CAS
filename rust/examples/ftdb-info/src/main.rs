use ftdb::load_ftdb_cache;
use std::path::Path;

fn run<T: AsRef<Path>>(path: T) -> ftdb::error::Result<()> {
    let fdb = load_ftdb_cache(path)?;
    println!("Metadata:");
    println!("  Directory: {}", fdb.directory());
    println!("  Release: {}", fdb.release());
    println!("  Module: {}", fdb.module());
    println!("  Version: {}", fdb.version());
    println!("Stats:");
    println!("  Files: {}", fdb.sources().len());
    println!("  Functions: {}", fdb.funcs().iter().count());
    println!("  Globals: {}", fdb.globals().len());
    println!("  Types: {}", fdb.types().len());
    Ok(())
}

fn main() -> Result<(), String> {
    let path = std::env::args()
        .nth(1)
        .ok_or_else(|| String::from("Missing path to .img file"))?;

    run(path).map_err(|e| format!("{}", e))
}
