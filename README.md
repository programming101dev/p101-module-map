# p101-module-map

`p101-module-map` helps students see the shape of a C project.

Its policy is intentionally structural: module size and naming, header/source
pairing, public API surface, include edges, cycles, and configured layering.
Error/environment ownership belongs to `p101-error-contract`; direct native
calls that bypass available wrappers belong to `p101-wrapper-audit`.

It uses the shared native `lib_c_facts` Clang analysis to parse `.c` and `.h`
files, groups files into modules by basename, and writes a Markdown report
showing:

- source/header pairs;
- public, private, and header-declared functions;
- local include relationships;
- optional layer-rule violations;
- modules that may be doing too much;
- non-static functions that look like private helpers;
- direct include cycles;
- likely utility dumping grounds such as `util.c`.

This is a teaching tool, not a proof engine. The C report generator no longer
tries to parse C itself. `lib_c_facts` owns the Clang AST pass. A saved
P101FACT stream remains available as an explicit replay input, so the module
map keeps a real parser while its policy and report logic stay readable.

Unreadable files, dangling symlinks, and missing optional layer files are skipped
or treated as non-fatal. The tool should report the project shape it can see
rather than abort because a build artifact or external include path wandered into
the scan.

## Usage

```sh
p101-module-map [-h] [-j] [-L] [-v] [-o <report>] [-l <layers.txt>] [-m <max-functions>] [-p <max-public>] [-i <facts.tsv> | -C <compile_commands.json>] [path...]
```

Examples:

```sh
p101-module-map src include
p101-module-map -o module-map.md src include
p101-module-map -l layers.txt src include
p101-module-map -m 8 -p 4 programs/simple-port-forwarder/src
p101-module-map -C build-clang/compile_commands.json src include
p101-module-map -L -C build-clang/compile_commands.json src include
p101-module-map -i source-facts.tsv -o module-map.md src include
p101-module-map -j -i source-facts.tsv -o module-map.json src include
```

With no paths, `p101-module-map` scans the current directory.

`-L` selects library mode. A library repo cannot prove that its public
functions, types, or macros are unused without scanning external consumers, and
its implementation must call the underlying APIs at wrapper boundaries.
Library mode therefore retains local structural checks while omitting those
closed-world findings. When a compile database is supplied, declarations for a
module with no active source translation unit are also excluded; this supports
source-controlled, non-installed platform placeholders. Installed-header/link
validation remains the build system's responsibility.

Function declarations and definitions are paired by C symbol across the
scanned project, not by matching filenames. This admits intentional split
implementations and umbrella headers. A source-only basename is reported only
when it exposes a non-static function with no scanned header declaration.

The tool automatically uses the current project's Clang compilation database
from `.last-build-dir` or `build-clang/compile_commands.json`. This preserves
sibling-library include roots, feature-test macros, and other project flags.
Use `-C` to select a different database explicitly.
Use `-i` to consume an existing P101FACT v4 snapshot without starting another
Clang AST pass. `-j` writes normalized findings with `id`, `severity`,
`location`, `message`, and `evidence`.

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
| `0` | Report was written with no findings |
| `1` | Report was written and contains one or more findings |
| `2` | Usage, file, parser, or other tool trouble |

## Build and check

Configure a compiler once, then run the gate:

```sh
./change-compiler.sh -c clang
./check.sh
```
