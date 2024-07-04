use ftdb::load;
use std::path::Path;

fn run<T: AsRef<Path>>(path: T, name: &str) -> ftdb::FtdbResult<()> {
    let fdb = load(path)?;
    for global in fdb.globals_by_name(name) {
        println!("{}:", global.location());
        println!("{}\n", global.defstring());
    }
    Ok(())
}

fn main() -> Result<(), String> {
    let path = std::env::args()
        .nth(1)
        .ok_or_else(|| String::from("Missing path to .img file"))?;
    let global_name = std::env::args()
        .nth(2)
        .ok_or_else(|| String::from("Missing global name"))?;
    run(path, &global_name).map_err(|e| format!("{}", e))
}
