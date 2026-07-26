# p101-module-map

`p101-module-map` helps students see the shape of a C project.

It scans `.c` and `.h` files, groups them into modules by basename, and writes a
Markdown report showing:

- source/header pairs;
- public, private, and header-declared functions;
- local include relationships;
- optional layer-rule violations;
- modules that may be doing too much;
- non-static functions that look like private helpers;
- direct include cycles;
- likely utility dumping grounds such as `util.c`.

This is a teaching tool, not a proof engine. Its v1 parser is intentionally
conservative and readable: it recognizes common C function signatures and
include lines, then turns those into a map students can discuss and improve.

## Usage

```sh
p101-module-map [-h] [-v] [-o <report.md>] [-l <layers.txt>] [-m <max-functions>] [-p <max-public>] [path...]
```

Examples:

```sh
p101-module-map src include
p101-module-map -o module-map.md src include
p101-module-map -l layers.txt src include
p101-module-map -m 8 -p 4 programs/simple-port-forwarder/src
```

With no paths, `p101-module-map` scans the current directory.

Layer files contain allowed local include edges, one per line:

```text
cli -> runner
runner -> model
report -> model
```

When `-l` is provided, any scanned local include edge not listed in the file is
reported as a teaching note.

## Exit status

| Status | Meaning |
| --- | --- |
| `0` | Report was written |
| `1` | Usage, file, or scan error |

## Build and check

Configure a compiler once, then run the gate:

```sh
./change-compiler.sh -c clang
./check.sh
```
