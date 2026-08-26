#!/usr/bin/env python3
"""Take llama-dspark through the whole-program path and check the numbers.

One call does what a dozen commands did while this was being worked out:
emit the corpus, link it, generate one module, build one library, and
count what the library holds against the four the ordinary build makes.
Every step prints what it produced, and the run fails if any of it
differs from what is expected.

The corpus is the part that catches people out.  A serialised AST is
written by one build of clang and can be read by that build alone; hand
one to another and all it says is "unable to load precompiled file",
which names neither the file nor the reason.  So the corpus carries a
stamp of what wrote it, this checks the stamp before reading a single
unit, and says what does not match.

  verify.py --ppcg-build DIR --clang DIR --llama DIR [--corpus DIR]

DIR for --ppcg-build is a build of this repository; --clang is the build
of the patched LLVM it was built against, which llvm-patches/README.md
describes; --llama is a checkout of llama-dspark configured the way
tests/whole-program/README.md says.
"""

import argparse
import concurrent.futures as cf
import hashlib
import json
import os
import subprocess
import sys
import time


def say(what, *rest):
    print(f"{what:<28}" + " ".join(str(r) for r in rest), flush=True)


def die(*what):
    print("verify: " + " ".join(str(w) for w in what), file=sys.stderr)
    sys.exit(2)


def tool(build, name):
    """The path of one of pet's programs inside a build of this repository."""
    p = os.path.join(build, "ThirdParty", "pet", name)
    if not os.path.exists(p):
        die(f"{p} is not there; is {build} a build of this repository?")
    return p


def clang_of(build):
    p = os.path.join(build, "bin", "clang")
    if not os.path.exists(p):
        die(f"{p} is not there; is {build} a build of LLVM?")
    return p


def stamp_of(emitter):
    """What identifies the build that writes a corpus.

    The emitter itself and the clang it is linked against: a serialised
    AST holds the version of the reader that may read it back, and these
    two are what decide it.
    """
    out = subprocess.run(["ldd", emitter], capture_output=True,
                         text=True).stdout
    libs = sorted(l.split("=>")[1].split("(")[0].strip()
                  for l in out.splitlines() if "clang" in l and "=>" in l)
    h = hashlib.sha256()
    for p in [emitter] + libs:
        try:
            st = os.stat(p)
            h.update(f"{p}:{st.st_size}:{int(st.st_mtime)}".encode())
        except OSError:
            h.update(p.encode())
    return {"emitter": emitter, "clang_libs": libs, "digest": h.hexdigest()[:16]}


def unit_list(path, llama):
    """The units named in the list, as the paths of their serialised form."""
    units = []
    for line in open(path):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        src = os.path.join(llama, line)
        units.append(src.replace("/", "%") + ".ast")
    return units


def emit(emitter, cdb, outdir, jobs):
    entries = json.load(open(cdb))
    seen, todo = set(), []
    for e in entries:
        f = e["file"]
        if "/engine/" not in f or f in seen:
            continue
        seen.add(f)
        todo.append((f, os.path.join(outdir, f.replace("/", "%") + ".ast")))

    os.makedirs(outdir, exist_ok=True)
    bad = []

    def one(src, out):
        r = subprocess.run([emitter, "--compile-commands", cdb, src, out],
                           capture_output=True, text=True)
        return src, r.returncode, r.stderr

    with cf.ThreadPoolExecutor(max_workers=jobs) as ex:
        for src, rc, err in ex.map(lambda a: one(*a), todo):
            if rc != 0:
                bad.append((src, err.strip().splitlines()[-1:]))
    return len(todo), bad


def run(*cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def defined_symbols(path):
    out = run("nm", "-D", "--defined-only", path).stdout
    return {l.split()[-1] for l in out.splitlines()
            if " T " in l or " D " in l or " B " in l}


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ppcg-build", required=True,
                    help="a build of this repository")
    ap.add_argument("--clang", required=True,
                    help="the build of patched LLVM it was built against")
    ap.add_argument("--llama", required=True,
                    help="a checkout of llama-dspark, configured")
    ap.add_argument("--llama-build", default="build-noavx",
                    help="its build directory, relative or absolute")
    ap.add_argument("--corpus", default=None,
                    help="where the serialised units live [work/corpus]")
    ap.add_argument("--work", default="/dev/shm/whole-program-verify",
                    help="where to put what is generated")
    ap.add_argument("--units", default=None,
                    help="the list of units [beside this script]")
    ap.add_argument("--reuse-corpus", action="store_true",
                    help="do not emit; read the corpus that is there")
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    ap.add_argument("--expect-units", type=int, default=51)
    ap.add_argument("--expect-refused", type=int, default=0)
    ap.add_argument("--expect-records", type=int, default=279)
    ap.add_argument("--expect-missing", type=int, default=0)
    args = ap.parse_args()

    emitter = tool(args.ppcg_build, "pet_emit_ast")
    linker = tool(args.ppcg_build, "pet_ast_link")
    irgen = tool(args.ppcg_build, "pet_linked_ir")
    cc = clang_of(args.clang)

    llama = os.path.abspath(args.llama)
    lbuild = args.llama_build
    if not os.path.isabs(lbuild):
        lbuild = os.path.join(llama, lbuild)
    cdb = os.path.join(lbuild, "compile_commands.json")
    if not os.path.exists(cdb):
        die(f"{cdb} is not there; configure llama-dspark first, see "
            "tests/whole-program/README.md")

    units_file = args.units or os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "llama-dspark.units")
    if not os.path.exists(units_file):
        die(f"{units_file} is not there")

    os.makedirs(args.work, exist_ok=True)
    corpus = args.corpus or os.path.join(args.work, "corpus")
    stamp_path = os.path.join(corpus, "written-by.json")
    stamp = stamp_of(emitter)

    say("ppcg build", args.ppcg_build)
    say("clang", cc)
    say("llama-dspark", llama)
    say("corpus", corpus)

    if args.reuse_corpus:
        if not os.path.exists(stamp_path):
            die(f"{corpus} carries no stamp, so what wrote it is not known; "
                "emit it again without --reuse-corpus")
        was = json.load(open(stamp_path))
        if was.get("digest") != stamp["digest"]:
            die("the corpus was written by another build and cannot be read "
                "back by this one.\n"
                f"  written by: {was.get('emitter')}\n"
                f"      against: {', '.join(was.get('clang_libs', []))}\n"
                f"  reading with: {stamp['emitter']}\n"
                f"       against: {', '.join(stamp['clang_libs'])}\n"
                "A serialised AST is readable only by the build that wrote "
                "it.  Emit the corpus again, or point --ppcg-build at the "
                "build that wrote this one.")
        say("corpus stamp", "matches", was["digest"])
    else:
        t = time.time()
        n, bad = emit(emitter, cdb, corpus, args.jobs)
        if bad:
            for src, err in bad[:5]:
                print(f"  failed: {src}\n    {' '.join(err)}",
                      file=sys.stderr)
            die(f"{len(bad)} of {n} units did not serialise")
        json.dump(stamp, open(stamp_path, "w"))
        say("corpus emitted", f"{n} units in {time.time() - t:.0f}s")

    units = unit_list(units_file, llama)
    missing = [u for u in units if not os.path.exists(os.path.join(corpus, u))]
    if missing:
        die(f"{len(missing)} units of the list are not in the corpus, "
            f"the first being {missing[0]}")
    paths = [os.path.join(corpus, u) for u in units]
    say("units listed", len(paths))

    r = run(linker, *paths)
    report = {}
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0] in ("units", "refused", "records"):
            report[parts[0]] = int(parts[1])
    if not report:
        die("the link said nothing:\n" + (r.stderr[-600:] or "(silence)"))
    say("link", f"units {report.get('units')} refused "
        f"{report.get('refused')} records {report.get('records')} "
        f"rc {r.returncode}")

    ll = os.path.join(args.work, "whole.ll")
    t = time.time()
    r2 = run(irgen, ll, *paths)
    if r2.returncode != 0 or not os.path.exists(ll):
        die("the module was not generated:\n" + r2.stderr[-600:])
    say("module", f"{os.path.getsize(ll) / 1e6:.1f} MB "
        f"in {time.time() - t:.0f}s")

    obj = os.path.join(args.work, "whole.o")
    so = os.path.join(args.work, "libwhole.so")
    r3 = run(cc, "-c", "-O2", "-o", obj, ll)
    if r3.returncode:
        die("the module did not assemble:\n" + r3.stderr[-600:])
    r4 = run(cc, "-shared", "-fopenmp", "-o", so, obj, "-lm", "-lstdc++")
    if r4.returncode:
        die("the library did not link:\n" + r4.stderr[-600:])
    say("library", f"{os.path.getsize(so) / 1e6:.1f} MB")

    ordinary = set()
    binroot = os.path.join(lbuild, "bin")
    for name in ("libllama.so", "libggml.so", "libggml-base.so",
                 "libggml-cpu.so"):
        p = os.path.join(binroot, name)
        if not os.path.exists(p):
            die(f"{p} is not there; build llama-dspark the ordinary way "
                "first, so that there is something to compare against")
        ordinary |= defined_symbols(p)
    whole = defined_symbols(so)
    absent = sorted(ordinary - whole)
    say("symbols", f"{len(ordinary)} in the four, {len(whole)} in the one, "
        f"{len(absent)} missing")
    for a in absent[:10]:
        print("   missing:", a)

    wrong = []
    if report.get("units") != args.expect_units:
        wrong.append(f"units {report.get('units')} != {args.expect_units}")
    if report.get("refused") != args.expect_refused:
        wrong.append(f"refused {report.get('refused')} != {args.expect_refused}")
    if report.get("records") != args.expect_records:
        wrong.append(f"records {report.get('records')} != {args.expect_records}")
    if r.returncode != 0:
        wrong.append(f"the link exited {r.returncode}")
    if len(absent) != args.expect_missing:
        wrong.append(f"{len(absent)} symbols missing, expected "
                     f"{args.expect_missing}")

    print()
    if wrong:
        print("verify: FAILED")
        for w in wrong:
            print("  ", w)
        return 1
    print("verify: the whole-program path holds")
    return 0


if __name__ == "__main__":
    sys.exit(main())
