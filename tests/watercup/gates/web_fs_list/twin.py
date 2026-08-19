"""twin for web_fs_list.

Traces the SAME decision structure as the C harness of hermes_cli/web_server.py:
fs_list — the hidden-entry filter, the isDirectory flag, and the sort key. Uses
the identical in-memory TREE so the cup reflects the algorithm, not the disk.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
from watercup_py import Cup

TREE = [
    ("/root", True), ("/root/.git", True), ("/root/file_a.txt", False),
    ("/root/file_B.txt", False), ("/root/sub", True), ("/root/sub/inner.md", False),
    ("/root/zebra", True),
]
HIDDEN = {".git", ".env"}

def _list(root):
    n = 0; dirs = 0
    for path, is_dir in TREE:
        if not path.startswith(root): continue
        rest = path[len(root):]
        if not rest.startswith("/"): continue
        name = rest[1:]
        if "/" in name: continue
        yield name, is_dir

def run(inputs):
    cups = []
    for root in inputs:
        c = Cup("fs_list")
        c.valstr(root if root is not None else "(null)")
        n = 0; dirs = 0
        for name, is_dir in _list(root):
            c.loop()
            if name in HIDDEN:
                c.branch(True); continue
            c.branch(False)
            n += 1
            if is_dir:
                c.branch(True); dirs += 1
            else:
                c.branch(False)
        c.valstr("sort:(not_dir,name_lower,name)")
        c.valstr("n="); c.valstr("d=")
        cups.append(c.finalize())
    return cups
