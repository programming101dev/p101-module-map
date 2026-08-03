#!/usr/bin/env bash
set -euo pipefail

tool=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/p101-module-map-test.XXXXXX")
trap 'rm -rf "$work"' EXIT

expect() {
  wanted=$1
  shift
  set +e
  "$tool" "$@" >"$work/stdout" 2>"$work/stderr"
  got=$?
  set -e
  [ "$got" -eq "$wanted" ] || {
    cat "$work/stderr" >&2
    return 1
  }
}

cat >"$work/facts.tsv" <<'FACTS'
ordinary text
P101FACT	2	FILE	src/alpha.c	alpha	0	1
P101FACT	2	FILE	src/alpha-extra.c	alpha	0	1
P101FACT	2	FILE	include/alpha.h	alpha	1	1
P101FACT	2	FILE	include/alpha-extra.h	alpha	1	1
P101FACT	2	FILE	src/beta.c	beta	0	1
P101FACT	2	FILE	src/main.c	main	0	1
P101FACT	2	FILE	src/util.c	util	0	1
P101FACT	2	FILE	include/gamma.h	gamma	1	1
P101FACT	2	INCLUDE	src/alpha.c	alpha	0	2	alpha.h	1
P101FACT	2	INCLUDE	src/alpha.c	alpha	0	3	stdio.h	0
P101FACT	2	INCLUDE	src/alpha.c	alpha	0	4	beta.h	1
P101FACT	2	INCLUDE	src/alpha.c	alpha	0	5	missing.h	1
P101FACT	2	INCLUDE	src/alpha.c	alpha	0	6	linux/input.h	0
P101FACT	2	INCLUDE	src/alpha.c	alpha	0	7	mach/mach.h	0
P101FACT	2	INCLUDE	src/alpha.c	alpha	0	8	windows.h	0
P101FACT	2	INCLUDE	src/alpha.c	alpha	0	9	sys/event.h	0
P101FACT	2	INCLUDE	src/alpha.c	alpha	0	10	sys/kqueue.h	0
P101FACT	2	INCLUDE	src/alpha.c	alpha	0	11	sys/sysctl.h	0
P101FACT	2	INCLUDE	src/beta.c	beta	0	2	alpha.h	1
P101FACT	2	FUNCTION	src/alpha.c	alpha	0	10	alpha_public	0	0
P101FACT	2	FUNCTION	include/alpha.h	alpha	1	5	alpha_public	0	1
P101FACT	2	FUNCTION	src/alpha.c	alpha	0	20	alpha_private	0	0
P101FACT	2	FUNCTION	src/alpha.c	alpha	0	30	alpha_static	1	0
P101FACT	2	FUNCTION	src/beta.c	beta	0	10	beta_large	0	0
P101FACT	2	FUNCTION	src/main.c	main	0	2	main	0	0
P101FACT	2	FUNCTION	src/main.c	main	0	3	main_a	1	0
P101FACT	2	FUNCTION	src/main.c	main	0	4	main_b	1	0
P101FACT	2	FUNCTION	src/main.c	main	0	5	main_c	1	0
P101FACT	2	FUNCTION	src/util.c	util	0	2	util_a	1	0
P101FACT	2	FUNCTION	src/util.c	util	0	3	util_b	1	0
P101FACT	2	FUNCTION	src/util.c	util	0	4	util_c	1	0
P101FACT	2	FUNCTION	src/util.c	util	0	5	util_d	1	0
P101FACT	2	FUNCTION	src/util.c	util	0	6	util_e	1	0
P101FACT	2	CALL	src/beta.c	beta	0	11	alpha_public	0	0
P101FACT	2	CALL	src/alpha.c	alpha	0	31	p101_error_create	0	0
P101FACT	2	CALL	src/alpha.c	alpha	0	32	p101_error_has_error	0	0
P101FACT	2	CALL	src/alpha.c	alpha	0	32	p101_error_is_error	0	0
P101FACT	2	CALL	src/alpha.c	alpha	0	32	p101_error_reset	0	0
P101FACT	2	CALL	src/alpha.c	alpha	0	33	p101_error_destroy	0	0
P101FACT	2	CALL	src/alpha.c	alpha	0	34	p101_env_create	0	0
P101FACT	2	CALL	src/alpha.c	alpha	0	35	p101_env_destroy	0	0
P101FACT	2	CALL	src/beta.c	beta	0	14	p101_error_create	0	0
P101FACT	2	CALL	src/beta.c	beta	0	15	p101_env_create	0	0
P101FACT	2	CALL	src/beta.c	beta	0	16	malloc	0	0
P101FACT	2	FUNCTION	include/alpha.h	alpha	1	8	missing_definition	0	1
P101FACT	2	FUNCTION	include/gamma.h	gamma	1	2	gamma_api	0	1
P101FACT	2	MACRO	include/gamma.h	gamma	1	3	GAMMA_LIMIT
P101FACT	2	TYPE	include/gamma.h	gamma	1	4	gamma_type
P101FACT	2	TYPE	include/alpha.h	alpha	1	6	alpha_type
P101FACT	2	MACRO	include/alpha.h	alpha	1	7	ALPHA_LIMIT
P101FACT	2	NOTE	src/beta.c	beta	0	12	ERROR_USE
P101FACT	2	NOTE	src/beta.c	beta	0	13	ERROR_CHECK
FACTS

cat >"$work/layers.txt" <<'LAYERS'
# allowed dependencies

not an edge
beta -> alpha
LAYERS

expect 0 --help
expect 0 -h
expect 2 -Z
expect 2 -i
P101_MODULE_MAP_TEST_OPTION=: expect 2
P101_MODULE_MAP_TEST_OPTION='?' expect 2
P101_MODULE_MAP_TEST_OPTION=@ expect 2
expect 2 -m nope -i "$work/facts.tsv"
expect 2 -p nope -i "$work/facts.tsv"
expect 2 -i "$work/facts.tsv" -C compile_commands.json
expect 1 -i "$work/facts.tsv" -l "$work/layers.txt" -m 1 -p 1 src include
for diagnostic_id in \
  P101-MOD-001 \
  P101-MOD-002 \
  P101-MOD-003 \
  P101-MOD-004 \
  P101-MOD-005 \
  P101-MOD-006 \
  P101-MOD-007 \
  P101-MOD-008 \
  P101-MOD-009 \
  P101-MOD-010 \
  P101-MOD-011 \
  P101-MOD-012 \
  P101-MOD-013
do
  grep -q "$diagnostic_id" "$work/stdout"
done
expect 1 -j -L -v -i "$work/facts.tsv" -l "$work/layers.txt" -m 1 -p 1 -o "$work/report.json"
test -s "$work/report.json"
expect 1 -L -i "$work/facts.tsv"

printf 'ordinary text\n' >"$work/empty.tsv"
expect 2 -i "$work/empty.tsv"
printf 'P101FACT\t2\tFILE\tbroken\n' >"$work/bad.tsv"
expect 2 -i "$work/bad.tsv"
expect 2 -i "$work/missing.tsv"
expect 2 -i "$work/facts.tsv" -o /tmp
expect 2 -i "$work/facts.tsv" -l "$work/missing-layers.txt"

{
  printf 'P101FACT\t2\tFILE\t'
  head -c 5000 /dev/zero | tr '\0' x
  printf '\nP101FACT\t2\tFILE\tsrc/main.c\tmain\t0\t1\n'
} >"$work/long.tsv"
expect 0 -i "$work/long.tsv"
