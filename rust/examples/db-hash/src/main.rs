use data_encoding::HEXLOWER;
use ftdb::InnerRef;
use openssl::sha::Sha256;
use std::env;
use std::error::Error;

fn main() -> Result<(), Box<dyn Error>> {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        println!("Usage: {} IMG_FILE", &args[0]);
        return Ok(());
    }

    let ftdb = ::ftdb::load(&args[1]).expect("Failed to open ftdb");

    let mut hasher = Sha256::new();

    ftdb.funcs_iter().for_each(|f| {
        hasher.update(f.body().as_bytes());
        hasher.update(f.unpreprocessed_body().as_bytes());

        f.derefs().iter().for_each(|d| {
            let kind = (d.inner_ref().kind as u64).to_le_bytes();
            hasher.update(&kind);
        });

        f.types()
            .iter()
            .filter_map(|id| ftdb.type_by_id(*id))
            .filter_map(|t| t.def())
            .for_each(|def| hasher.update(def.as_bytes()));
    });

    ftdb.globals_iter()
        .filter_map(|g| ftdb.type_by_id(g.type_()))
        .for_each(|t| {
            if let Some(def) = t.def() {
                hasher.update(def.as_bytes());
            }
        });

    let result = hasher.finish();
    println!("hash: {}", HEXLOWER.encode(result.as_ref()));

    Ok(())
}
