use ftdb::Linkage;
use ftdb_test::load_test_case;

#[test]
fn source_info_works() {
    let testcase = load_test_case("001-hello-world");
    let sources = testcase.db.sources();
    assert_eq!(sources.len(), 1, "Single main.c source expected");
    let expected_path = testcase.test_file("main.c");
    let expected_path = expected_path.to_str().unwrap();
    assert_eq!(sources[0], expected_path);
}

#[test]
fn module_works() {
    let testcase = load_test_case("001-hello-world");
    let module = testcase.db.module();
    assert_eq!(module, "001-hello-world");
}

#[test]
fn directory_works() {
    let testcase = load_test_case("001-hello-world");
    let directory = testcase.db.directory();
    let expected = testcase.root_dir();
    assert_eq!(directory, expected);
}

#[test]
fn hello_func_exists() {
    let testcase = load_test_case("001-hello-world");
    let func = testcase.db.funcs_by_name("hello").next();
    assert!(
        func.is_some(),
        "Func hello is expected to be found in funcs section"
    );
    let func = func.unwrap();
    assert_eq!(func.name(), "hello");
    assert_eq!(func.nargs(), 1);
}

#[test]
fn main_func_exists() {
    let testcase = load_test_case("001-hello-world");
    let func = testcase.db.funcs_by_name("main").next();
    assert!(
        func.is_some(),
        "Func main is expected to be found in funcs section"
    );
    let func = func.unwrap();
    assert_eq!(func.name(), "main");
    assert_eq!(func.nargs(), 0);
    assert_eq!(func.declbody(), "int main()");
    assert_eq!(func.signature(), "main int ()");
    assert!(!func.is_variadic());
}

#[test]
fn main_func_calls_hello() {
    let testcase = load_test_case("001-hello-world");
    let main_func = testcase.expect_func("main");
    let hello_func = testcase.expect_func("hello");
    let hello_func_id = hello_func.id();
    assert!(
        main_func.call_ids().contains(&hello_func_id),
        "main() expected to call hello() function"
    );
}

#[test]
fn hello_func_calls_printf() {
    let testcase = load_test_case("001-hello-world");
    let hello_func = testcase.expect_func("hello");
    let printf_func = testcase.expect_funcdecl("printf");
    let printf_func_id = printf_func.id();
    assert!(
        hello_func.call_ids().contains(&printf_func_id),
        "hello() expected to call printf() function"
    );
}

#[test]
#[allow(non_snake_case)]
fn WORLD_global_exists() {
    let testcase = load_test_case("001-hello-world");
    let g = testcase.expect_global("WORLD");
    assert_eq!(g.name(), "WORLD");
    assert_eq!(g.init(), Some(r#""world""#));
    assert_eq!(g.linkage(), Linkage::Internal);
}

#[test]
fn printf_from_stdio_exists() {
    let testcase = load_test_case("001-hello-world");
    let f = testcase.expect_funcdecl("printf");

    assert_eq!(f.name(), "printf");
    assert_eq!(f.linkage(), Linkage::External);
    assert!(f.is_variadic(), "printf() expected variadic");
    assert_eq!(f.nargs(), 1, "printf() has 1 non-variadic arg");
}
