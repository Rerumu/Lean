## Lean

`lean` is an extension of `lau` which seeks to transpile Lua 5.4 bytecode into a C representation that can be bundled with the Lua runtime. It's simple enough to just replace `lua.c` and run the make file.

The program is currently implemented as a command line tool, and usage can be observed via `lean -h`.
