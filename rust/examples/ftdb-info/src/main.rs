use ftdb::FtdbCollection;
use std::path::Path;

fn run<T: AsRef<Path>>(path: T) -> ftdb::FtdbResult<()> {
    let fdb = ::ftdb::load(path)?;
    println!("Metadata:");
    println!("  Directory:  {}", fdb.directory());
    println!("  Release:    {}", fdb.release());
    println!("  Module:     {}", fdb.module());
    println!("  Version:    {}", fdb.version());

    println!("\nStats:");
    println!("  Files:      {:>10}", fdb.sources().len());
    println!("  Funcdecls:  {:>10}", fdb.funcdecls().len());
    println!("  Functions:  {:>10}", fdb.funcs().len());
    println!("  Globals:    {:>10}", fdb.globals().len());
    println!("  Types:      {:>10}", fdb.types().len());
    println!("  Unresolved: {:>10}", fdb.unresolved_funcs().len());

    Ok(())
}

fn main() -> Result<(), String> {
    let path = std::env::args()
        .nth(1)
        .ok_or_else(|| String::from("Missing path to .img file"))?;

    run(path).map_err(|e| format!("{}", e))
}
