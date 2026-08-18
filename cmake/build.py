#!/usr/bin/env python3
"""Build, and say whether the result holds what it was meant to.

A build reporting success does not mean the binary is the one the
sources describe, and running one that is not wastes far more than the
build did.  Three conclusions drawn over one afternoon here were wrong
for that reason: an executable still held a call that had been deleted
from its source, because an edit meant to remove it had silently not
applied, and every measurement taken afterwards described the old code.

The build system was not at fault and neither was its report.  What was
missing was any statement of what the result should contain.  So this
takes one:

    build.py --target pet_linked_ir --expect-no setFileManager
    build.py --target ppcg --expect ppcg_reduction_dependences

and looks for it in the binary, with nm, once the build is done.  A
build that succeeds and does not hold what was asked for exits 2, which
is what tells a caller the difference.

The exit status is the build's otherwise.
"""

import argparse
import os
import subprocess
import sys
import time


def symbols(path):
    """Every name the binary mentions, defined or wanted."""
    seen = set()
    for flags in (["-D", "--defined-only"], ["-u"], []):
        out = subprocess.run(["nm"] + flags + ["--format=posix", path],
                             capture_output=True, text=True)
        for line in out.stdout.splitlines():
            parts = line.split()
            if parts:
                seen.add(parts[0])
    return seen


def artefact(build_dir, target):
    """Where the build put "target"."""
    found = []
    for dirpath, dirnames, filenames in os.walk(build_dir):
        dirnames[:] = [d for d in dirnames if d != "CMakeFiles"]
        for name in filenames:
            path = os.path.join(dirpath, name)
            if not os.access(path, os.X_OK) or os.path.isdir(path):
                continue
            if name.endswith((".cmake", ".txt", ".json", ".py", ".sh")):
                continue
            if target and name != target:
                continue
            found.append(path)
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", nargs="?", default="build")
    parser.add_argument("--target", "-t", default=None)
    parser.add_argument("--jobs", "-j", type=int, default=os.cpu_count())
    parser.add_argument("--expect", action="append", default=[],
                        help="a name the built binary has to mention")
    parser.add_argument("--expect-no", action="append", default=[],
                        help="a name it may not mention")
    args = parser.parse_args()

    command = ["cmake", "--build", args.build_dir, "-j", str(args.jobs)]
    if args.target:
        command += ["--target", args.target]

    started = time.time()
    result = subprocess.run(command)
    took = time.time() - started

    if result.returncode != 0:
        print("build failed with %d after %.0f s" % (result.returncode, took))
        return result.returncode

    if not args.expect and not args.expect_no:
        print("built in %.0f s" % took)
        return 0

    built = artefact(args.build_dir, args.target)
    if not built:
        print("build reported success and produced no %s" % args.target)
        return 2

    wrong = []
    for path in built:
        names = symbols(path)
        for want in args.expect:
            if not any(want in name for name in names):
                wrong.append("%s does not mention %s" % (path, want))
        for avoid in args.expect_no:
            if any(avoid in name for name in names):
                wrong.append("%s still mentions %s" % (path, avoid))

    if wrong:
        print("built in %.0f s, and the result is not what was asked for:"
              % took)
        for line in wrong:
            print("  %s" % line)
        return 2

    print("built in %.0f s, and it holds what was asked for" % took)
    return 0


if __name__ == "__main__":
    sys.exit(main())
