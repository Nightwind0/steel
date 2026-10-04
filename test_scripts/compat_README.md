# Interpreter compatibility probes

These small scripts are deterministic probes for syntax and runtime behavior. Run them from the repository root with the C++ interpreter and Rust interpreter:

```sh
./steeli test_scripts/compat_values.steel
cargo run --manifest-path rust-steel/Cargo.toml -- test_scripts/compat_values.steel
```

Repeat with each `compat_*.steel` file. `compat_negative_parse.steel`, `compat_negative_runtime.steel`, and `compat_negative_type.steel` are expected to fail in both interpreters; the other files should complete successfully. Compare stdout and exit status, allowing for the C++ interpreter's integer/real formatting and different error wording.

The probes intentionally avoid host-only APIs, randomness, and file-provider behavior so differences are attributable to the language implementation.
