#!/usr/bin/env python3
"""Differential oracle for agent/checkpoints/pruning.py (gate w19_pruning_c083bd).

The live ``Pruner`` dataclass is driven with a scripted ``run_git`` / ``dir_size``; the decision
is read back from the git calls it actually made (every ``log -1`` is one kept snapshot, every
``update-ref`` is one rewritten ref). ``sep`` stands in for git's %x00 separator byte.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
SRC = Q + "/agent/checkpoints/pruning.py"
GATE = "w19_pruning_c083bd"

spec = importlib.util.spec_from_file_location("pruning_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

STORE = Path("C:/Users/eman5/AppData/Local/hermes/cache/scratch/prune_store")
METADATA = "TREE" + "\x00" + "A-DATE" + "\x00" + "C-DATE" + "\x00" + "BODY"


class Rig:
    def __init__(self, commits_map, sizes, log_text=None, fail_err=None, first=None):
        self.commits_map = commits_map or {}
        self.sizes = list(sizes or [])
        self.size_i = 0
        self.first = first
        self.last = 0.0 if first is None else first
        self.log_text = log_text if log_text is not None else METADATA
        self.fail_err = fail_err
        self.calls = []

    def dir_size(self, _p):
        if self.first is not None:
            self.last = self.first
            self.first = None
            return self.last
        if self.size_i < len(self.sizes):
            self.last = self.sizes[self.size_i]
            self.size_i += 1
        return self.last

    def run_git(self, args, store, cwd, **kwargs):
        self.calls.append((list(args), kwargs))
        head = args[0]
        if head == "rev-list":
            if self.fail_err is not None:
                return False, "", self.fail_err
            return True, "\n".join(self.commits_map.get(args[-1], [])), ""
        if head == "for-each-ref":
            return True, "\n".join(self.commits_map), ""
        if head == "log":
            return True, self.log_text, ""
        if head == "commit-tree":
            return True, "newsha", ""
        return True, "", ""

    def pruner(self):
        return live.Pruner(run_git=self.run_git, store=STORE, cwd="", timeout=1,
                           dir_size=self.dir_size, refs_prefix="refs/ck/")

    def summarize(self):
        logs = [c[0] for c in self.calls if c[0][0] == "log"]
        ups = [c[0] for c in self.calls if c[0][0] == "update-ref"]
        return logs, ups


def commits_of(refs):
    return {ref: list(cs) for ref, cs in refs}


def run_case(r):
    f = r.get("func")
    if f == "lock_path":
        return str(live.store_lock_path(Path(r.get("base", ""))))
    if f == "parse_metadata":
        sep = r.get("sep", "|")
        text = (r.get("text") or "").replace(sep, chr(0))
        sha = r.get("sha", "sha0")
        rig = Rig({}, [], log_text=text)
        p = rig.pruner()
        try:
            p._rewrite("refs/ck/x", [sha], [sha], gc=False)
        except live.PruneError as e:
            return {"ok": False, "error": str(e)}
        for args, kwargs in rig.calls:
            if args[0] == "commit-tree":
                env = kwargs.get("extra_env") or {}
                return {"ok": True, "tree": args[1],
                        "author_date": env.get("GIT_AUTHOR_DATE"),
                        "committer_date": env.get("GIT_COMMITTER_DATE"),
                        "message": args[4].replace(chr(0), sep)}
        return {"ok": True}
    if f == "commits":
        rig = Rig({}, [], fail_err=r.get("git_error"))
        rig.commits_map = {r.get("ref", ""): (r.get("output") or "").splitlines()}
        p = rig.pruner()
        try:
            return {"ok": True, "commits": p._commits(r.get("ref", ""))}
        except live.PruneError as e:
            return {"ok": False, "error": str(e)}
    if f == "trim":
        commits = r.get("commits") or []
        rig = Rig({"refs/ck/x": commits}, [])
        p = rig.pruner()
        try:
            p.trim("refs/ck/x", r.get("keep_count", 1))
        except live.PruneError as e:
            return {"ok": False, "error": str(e)}
        logs, ups = rig.summarize()
        keep = [c[3] for c in logs]
        return {"ok": True, "action": "rewrite" if ups else "none", "keep": keep}
    if f == "drop":
        cmap = commits_of(r.get("refs") or [])
        rig = Rig(cmap, [], first=r.get("size", 0))
        p = rig.pruner()
        changed = p.drop_one_round(r.get("cap", 0))
        logs, ups = rig.summarize()
        return {"changed": bool(changed), "dropped": [c[1] for c in ups]}
    if f == "enforce":
        cmap = commits_of(r.get("refs") or [])
        rig = Rig(cmap, r.get("sizes") or [], first=r.get("size", 0))
        p = rig.pruner()
        result = p.enforce_size(r.get("cap", 0))
        logs, ups = rig.summarize()
        return {"result": bool(result), "rewrites": len(ups), "kept": [c[3] for c in logs]}
    return "UNKNOWN_FUNC"


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
