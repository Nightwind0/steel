# Steel in Rust (early implementation)

This crate is a standalone Rust implementation of the Steel scripting language, developed alongside the original C++ interpreter. It currently includes a lexer, Pratt expression parser, dynamically typed runtime, lexical scopes and closures, control flow, arrays/hashes, string interpolation, selected standard functions, and `require`.

Build and run a script with:

```sh
cargo build --release
cargo run -- path/to/script.steel
```

Pass `-` as the script path to read from standard input, or `--no-exec` to parse without running. The implementation is not yet feature-complete or behavior-compatible with the C++ interpreter. In particular, the C++ standard library and I/O functions, complete namespace semantics, and several edge cases still need to be ported. Use the original interpreter for production scripts for now.
