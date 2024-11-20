# FTDB rust bindings

## Setup

ftdb_c is a custom library probably not present on your system. You must 
ensure the `libftdb_c.so` file exists in a PATH discoverable by the linker.

The fastest way to build the library:

```bash
# from the CAS root directory
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -- -j10 ftdb_c
```

Then:
- Copy `libftdb_c.so` to your system lib, or
- Add a path to this lib to `/etc/ld.so.conf.d/ftdb.conf and run `ldconfig`

## Building

There are two ways to add the ftdb library. You might want to use the main 
registry:

```toml
# This is your Cargo.toml file
[dependencies]
ftdb = "0.11.0"
```

The other way is to specify the CAS repository directly:

```toml
# This is your Cargo.toml file
[dependencies.ftdb]
git = "https://github.com/Samsung/CAS.git"
branch = "master"
version = "0.11.0"
```

# License

Details are provided in the `rust/LICENSE` file.
