#!/usr/bin/env python3
"""Emit one pet .ast per engine translation unit of llama-dspark.

The units have to be emitted by pet's own pet_emit_ast, reading them with
pet's include paths, macros and predefines: a unit emitted by anything
else describes a different program than the one pet sees.  The flags each
unit is really built with come from the compilation database.
"""

import argparse
import concurrent.futures as cf
import json
import os
import subprocess
import sys


def emit(tool, cdb, src, out):
    cmd = [tool, "--compile-commands", cdb, src, out]
    r = subprocess.run(cmd, capture_output=True, text=True)
    return src, out, r.returncode, r.stderr


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--tool", required=True)
    p.add_argument("--cdb", required=True, help="compile_commands.json")
    p.add_argument("--outdir", required=True)
    p.add_argument("--filter", default="/engine/",
                   help="only files whose path contains this")
    p.add_argument("--jobs", type=int, default=os.cpu_count())
    args = p.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    entries = json.load(open(args.cdb))
    seen = set()
    todo = []
    for e in entries:
        f = e["file"]
        if args.filter not in f or f in seen:
            continue
        seen.add(f)
        name = f.split("/llama-dspark/", 1)[-1].replace("/", "%") + ".ast"
        todo.append((f, os.path.join(args.outdir, name)))

    print(f"{len(todo)} units to emit into {args.outdir}", flush=True)

    ok = bad = 0
    with cf.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        futs = [ex.submit(emit, args.tool, args.cdb, s, o) for s, o in todo]
        for fut in cf.as_completed(futs):
            src, out, rc, err = fut.result()
            if rc == 0 and os.path.exists(out):
                ok += 1
            else:
                bad += 1
                print(f"FAILED rc={rc} {src}", flush=True)
                for line in err.strip().splitlines()[:4]:
                    print(f"    {line}", flush=True)

    print(f"emitted {ok}, failed {bad}")
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
