#!/usr/bin/env python3
"""Create a Windows directory junction (mklink /J).

Why not `ln -s`: MSYS2's ln COPIES when it cannot create a symlink, which copied
the whole developer home -- 13 GB including .git-credentials -- into the oracle
build tree on every run. Why not `mklink /D`: a real symlink needs
SeCreateSymbolicLinkPrivilege (admin or Developer Mode). A junction needs
neither and is transparent to POSIX path resolution.

Usage: mklink_dir.py <link> <target>
"""
import os
import shutil
import subprocess
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: mklink_dir.py <link> <target>", file=sys.stderr)
        return 2
    link, target = sys.argv[1], sys.argv[2]
    if not os.path.isdir(target):
        print(f"mklink_dir: target is not a directory: {target}", file=sys.stderr)
        return 1

    # A junction cannot replace an existing directory in place.
    if os.path.lexists(link):
        if os.path.islink(link) or not os.path.isdir(link):
            try:
                os.remove(link)
            except OSError:
                shutil.rmtree(link, ignore_errors=True)
        else:
            shutil.rmtree(link, ignore_errors=True)

    os.makedirs(os.path.dirname(link) or ".", exist_ok=True)
    # cmd needs Windows-separated paths; a list argv avoids shell quoting.
    r = subprocess.run(
        ["cmd", "/c", "mklink", "/J",
         link.replace("/", "\\"), target.replace("/", "\\")],
        capture_output=True, text=True)
    if r.returncode != 0 or not os.path.isdir(link):
        sys.stderr.write(r.stdout + r.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
