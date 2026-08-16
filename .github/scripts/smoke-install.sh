#!/bin/bash
# Check that an installed ppcg prefix is actually usable.
#
#   smoke-install.sh <prefix> [source-dir]
#
# "The build succeeded" and "the install produced something a user can use"
# are two different claims.  This checks the second one:
#
#   1. the prefix contains the versioned libraries and the two programs,
#   2. the installed ppcg runs, reports a version, and translates a C program
#      into something that still computes the same answers,
#   3. the installed libisl/libpet can be found through pkg-config and linked
#      against from outside the build tree.
#
# It is a script rather than a block of workflow YAML because it is the part
# of the CI worth running by hand: the same command reproduces a user's
# "I installed it and it does not work" without a GitHub runner.

set -euo pipefail

PREFIX=${1:?usage: smoke-install.sh <prefix> [source-dir]}
SRC=${2:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}

PREFIX=$(cd "$PREFIX" && pwd)
SRC=$(cd "$SRC" && pwd)

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

CC=${CC:-cc}

# The installed programs carry an RPATH into their own prefix, but the program
# built in step 3 does not, so it needs the library path regardless.
LIBDIR="$PREFIX/lib"
[ -d "$PREFIX/lib64" ] && LIBDIR="$PREFIX/lib64"
export LD_LIBRARY_PATH="$LIBDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PKG_CONFIG_PATH="$LIBDIR/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

step() { printf '\n=== %s\n' "$1"; }
fail() { printf 'smoke test failed: %s\n' "$1" >&2; exit 1; }

step "installed layout"
ls -l "$PREFIX/bin"
ls -l "$LIBDIR"
# The versioned sonames are part of what "self-contained prefix" means.  A
# bare libisl.so with no libisl.so.NN behind it would mean the install rules
# lost the version.
for soname in libisl.so libpet.so libimath.so; do
    ls "$LIBDIR/$soname".* >/dev/null 2>&1 \
        || fail "no versioned $soname in $LIBDIR"
done

step "ppcg --version"
"$PREFIX/bin/ppcg" --version

step "pet --version"
"$PREFIX/bin/pet" --version

step "translate examples/chemv.c with the installed ppcg"
"$PREFIX/bin/ppcg" --target=c --tile "$SRC/examples/chemv.c" -o "$WORK/chemv_ppcg.c"
test -s "$WORK/chemv_ppcg.c" || fail "ppcg produced no output"

step "build the translation and compare it against the original"
"$CC" -std=gnu99 -O2 -I"$SRC" -o "$WORK/chemv_ref" "$SRC/examples/chemv.c" -lm
"$CC" -std=gnu99 -O2 -I"$SRC" -I"$WORK" -o "$WORK/chemv_cand" "$WORK/chemv_ppcg.c" -lm
"$WORK/chemv_ref" > "$WORK/ref.out"
"$WORK/chemv_cand" > "$WORK/cand.out"
# chemv accumulates its floating point results in a different order once the
# loops have been restructured, so it is compared with a tolerance, exactly
# as the ctest suite compares it.
python3 "$SRC/compare_outputs.py" "$WORK/ref.out" "$WORK/cand.out" true

step "pkg-config metadata"
# Checked before the link below, because an empty ${libdir} turns into a bare
# "-L" on the command line, which silently eats the next argument and makes
# the compiler complain about something unrelated.
for mod in isl pet; do
    pkg-config --exists "$mod" || fail "pkg-config cannot find $mod.pc"
    for var in libdir includedir; do
        value=$(pkg-config --variable="$var" "$mod")
        if [ -z "$value" ]; then
            fail "$mod.pc defines an empty $var, so its -L/-I flags are unusable"
        fi
        printf '%s.pc %s = %s\n' "$mod" "$var" "$value"
    done
    printf '%s %s: %s\n' "$mod" "$(pkg-config --modversion "$mod")" \
        "$(pkg-config --cflags --libs "$mod")"
done

step "link a program against the installed isl and pet"
cat > "$WORK/use_installed.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <isl/ctx.h>
#include <isl/set.h>
#include <pet.h>

/* isl_ctx_alloc_with_pet_options() comes from libpet and the set below from
 * libisl, so a program that links and runs proves both libraries were
 * installed in a usable state, headers included. */
int main(void)
{
	isl_ctx *ctx;
	isl_set *set;
	char *s;

	ctx = isl_ctx_alloc_with_pet_options();
	if (!ctx)
		return 1;
	set = isl_set_read_from_str(ctx, "{ [i] : 0 <= i < 10 }");
	set = isl_set_coalesce(set);
	s = isl_set_to_str(set);
	if (!s)
		return 1;
	printf("%s\n", s);
	free(s);
	isl_set_free(set);
	isl_ctx_free(ctx);
	return 0;
}
EOF
# pet.pc says "Requires: isl", so asking for pet alone has to bring in isl.
# shellcheck disable=SC2046
"$CC" -std=gnu99 -O2 -o "$WORK/use_installed" "$WORK/use_installed.c" \
    $(pkg-config --cflags --libs pet)
out=$("$WORK/use_installed")
echo "$out"
case "$out" in
    *"0 <= i <= 9"*) ;;
    *) fail "unexpected isl output: $out" ;;
esac

step "ocl_utilities installed for the generated OpenCL host code"
test -f "$PREFIX/share/ppcg/ocl_utilities.c" || fail "ocl_utilities.c not installed"
test -f "$PREFIX/share/ppcg/ocl_utilities.h" || fail "ocl_utilities.h not installed"

printf '\nsmoke test passed\n'
