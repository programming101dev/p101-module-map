# p101-module-map

`p101-module-map` helps students see the shape of a C project.

It asks `p101-wrapper-audit` to parse `.c` and `.h` files with Clang, consumes
the resulting plain TSV fact stream, groups files into modules by basename, and
writes a Markdown report showing:

- source/header pairs;
- public, private, and header-declared functions;
- local include relationships;
- optional layer-rule violations;
- modules that may be doing too much;
- non-static functions that look like private helpers;
- direct include cycles;
- likely utility dumping grounds such as `util.c`.

This is a teaching tool, not a proof engine. The C report generator no longer
tries to parse C itself; `p101-wrapper-audit` owns the Clang AST pass and emits
the shared fact stream parsed by `lib_c_facts`. That gives the module map a
real parser while keeping the report logic readable.

Unreadable files, dangling symlinks, and missing optional layer files are skipped
or treated as non-fatal. The tool should report the project shape it can see
rather than abort because a build artifact or external include path wandered into
the scan.

## Usage

```sh
p101-module-map [-h] [-v] [-o <report.md>] [-l <layers.txt>] [-m <max-functions>] [-p <max-public>] [-C <compile_commands.json>] [-F <p101-wrapper-audit>] [path...]
```

Examples:

```sh
p101-module-map src include
p101-module-map -o module-map.md src include
p101-module-map -l layers.txt src include
p101-module-map -m 8 -p 4 programs/simple-port-forwarder/src
p101-module-map -C build-clang/compile_commands.json src include
p101-module-map -F ../p101-wrapper-audit/p101-wrapper-audit src include
```

With no paths, `p101-module-map` scans the current directory.

The tool automatically uses the current project's Clang compilation database
from `.last-build-dir` or `build-clang/compile_commands.json`. This preserves
sibling-library include roots, feature-test macros, and other project flags.
Use `-C` to select a different database explicitly.

The Clang fact tool is resolved in this order:

- `-F <tool>`;
- `P101_MODULE_MAP_FACT_TOOL`;
- `P101_WRAPPER_AUDIT`;
- the sibling workspace path `../p101-wrapper-audit/p101-wrapper-audit`;
- `p101-wrapper-audit` from `PATH`.

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
