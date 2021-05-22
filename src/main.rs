use codegen::gen::transpile;
use loader::load_lua_module;
use std::io::Result;

mod codegen;
mod common;
mod dumper;
mod loader;
mod splitter;

fn list_help() {
	println!("usage: lean [options]");
	println!("  -h | --help              show the help message");
	println!("  -t | --transpile [file]  transpile a bytecode file to C");
}

fn transpile_data(data: &[u8]) {
	let (trail, proto) = load_lua_module(data).expect("not valid Lua 5.4 bytecode");

	if !trail.is_empty() {
		panic!("trailing garbage in Lua file");
	}

	transpile(&mut std::io::stdout().lock(), &proto).unwrap();
}

fn main() -> Result<()> {
	let mut iter = std::env::args().skip(1);

	while let Some(val) = iter.next() {
		match val.as_str() {
			"-h" | "--help" => {
				list_help();
			}
			"-t" | "--transpile" => {
				let name = iter.next().expect("file name expected");
				let data = std::fs::read(name)?;

				transpile_data(&data);
			}
			opt => {
				panic!("unknown option `{}`", opt);
			}
		}
	}

	Ok(())
}
