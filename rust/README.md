# rust-ftdb

Repository for Rust bindings to a ftdb library.


## Setup

Building ftdb-rust is configured through environment variables.


### FTDB_INCLUDE_DIR

```bash
export FTDB_INCLUDE_DIR=/media/storage/dev/CAS-OSS
```

This env represents path to ftdb headers required to generate bindings.

### FTDB_LIB_DIR

```bash
export FTDB_LIB_DIR=/media/storage/dev/CAS-OSS/build
```

This env represents path directory containing `libftdb_c.so` file. Note that this 
path is only used for building purposes. To load shared binary during the 
program execution, put `.so` file in a file available to `ld` or use
`LD_LIBRARY_PATH` env.

## Adding dependency

To add ftdb, add to your `Cargo.toml` file following snippet:

```toml
[dependencies.ftdb]
git = "ssh://git@github.sec.samsung.net/CO7-SRPOL-Mobile-Security/rust-ftdb.git"
branch = "master"
```

## Running

When running, remember to put `libftdb_c.so` in a place available to `ld` or use
`LD_LIBRARY_PATH` env.

## Documentation

To generate (and open) documentation run:

```bash
export FTDB_INCLUDE_DIR=/media/storage/dev/CAS-OSS
cargo doc --open
```
For development purposes you might also want to add `--document-private-items`
option.

When working remotely you might want to skip `--open` flag and then serve 
generated files using python:

```bash
python3 -m http.server 5000 --directory target/doc/
```

And then open up `http://some_hostname:5000/ftdb/` where hostname could 
be localhost if using ssh tunnels/socks proxy or an IP/hostname of the server 
you're connected to.


## Example

To start working with ftdb library open up flattened database file:

```rust
use ftdb::load_ftdb_cache;

fn run() -> ftdb::error::Result<()> {
    let fdb = load_ftdb_cache(path)?;
    println!("Metadata:");
    println!("  Directory: {}", fdb.directory());
    println!("  Module: {}", fdb.module());
    println!("  Release: {}", fdb.release());
    println!("  Version: {}", fdb.version());
    println!("Stats:");
    println!("  Files: {}", fdb.sources().len());
    println!("  Functions: {}", fdb.funcs().iter().count());
    println!("  Globals: {}", fdb.globals().len());
    println!("  Types: {}", fdb.types().len());
    Ok(())
}
```

### rust-info

This example prints basic info from the ftdb file.

Compile:

```bash
env \
    FTDB_INCLUDE_DIR=/media/storage/dev/CAS-OSS/bas \
    FTDB_LIB_DIR=/media/storage/dev/CAS-OSS/build   \
    cargo build -p ftdb-info
```

Run:

```bash
env \
    LD_LIBRARY_PATH=/media/storage/dev/CAS-OSS/build \
    target/debug/ftdb-info -- /path/to/image/file.img
```

or compile and run:

```bash
env \
    FTDB_INCLUDE_DIR=/media/storage/dev/CAS-OSS/bas \
    FTDB_LIB_DIR=/media/storage/dev/CAS-OSS/build   \
    LD_LIBRARY_PATH=/media/storage/dev/CAS-OSS/build \
    cargo run -p ftdb-info -- /path/to/image/file.img
```

Example output:

```
Metadata:
  Directory: /media/storage/BUILDS/Q5_US_004WAKFY3OBOE_eng/vendor/android/kernel_platform/out/msm-kernel-kalama-consolidate/msm-kernel
  Release: F946USQU0AWC7-eng:b1a05272c74733b20d1459af644827e7d386d01b
  Module: vmlinux
  Version: 0.90
Stats:
  Files: 5524
  Functions: 263325
  Globals: 128638
  Types: 170142

```
