#!/usr/bin/env python3
"""sta_oracle_subagent_worktree.py — Python twin for tools/subagent_worktree.py.

The C harness injects a scripted git hook; this twin mirrors that scripting:
each op carries a "git" list of scripted responses
  {"rc":0,"stdout":"...","stderr":"..."}
consumed in order by _run_git. Ops:
  {"kind":"resolve_repo_root","path":"...","git":[...]}
  {"kind":"create","cwd":"...","id":"a1","git":[...],"exists":true}
  {"kind":"mark_unproven","payload":{...},"reason":"rev-list exit 128","unmeasured":"commits"}
  {"kind":"unproven_payload","info":{...},"reason":"finalize raised"}
  {"kind":"finalize","info":{...},"prune":true,"git":[...]}
  {"kind":"context_note","info":{"path":"/w","branch":"b"}}
"""
import json
import sys


class ScriptedGit:
    def __init__(self, script):
        self.script = list(script or [])
        self.calls = []

    def run(self, args, cwd, timeout=30):
        self.calls.append(list(args))
        if not self.script:
            return type("R", (), {"returncode": -1, "stdout": "", "stderr": "script exhausted"})()
        r = self.script.pop(0)
        return type("R", (), {"returncode": r.get("rc", 0), "stdout": r.get("stdout", ""),
                              "stderr": r.get("stderr", "")})()


def make_module(git):
    import types
    mod = types.ModuleType("swt")
    mod._GIT_TIMEOUT = 30
    mod._WORKTREES_DIRNAME = ".worktrees"
    mod._BRANCH_NAMESPACE = "hermes-subagent"

    def _run_git(args, cwd, timeout=mod._GIT_TIMEOUT):
        return git.run(args, cwd, timeout)

    mod._run_git = _run_git

    def resolve_repo_root(path):
        if not path:
            return None
        import os
        candidate = os.path.abspath(os.path.expanduser(str(path)))
        if not os.path.isdir(candidate):
            return None
        result = _run_git(["rev-parse", "--show-toplevel"], cwd=candidate)
        if result.returncode != 0:
            return None
        root = result.stdout.strip()
        return root or None

    def create_subagent_worktree(parent_cwd, subagent_id=None):
        repo_root = resolve_repo_root(parent_cwd)
        if not repo_root:
            return None
        import os, uuid
        short_id = (subagent_id or uuid.uuid4().hex[:8]).replace("/", "-")
        wt_name = f"subagent-{short_id}"
        branch = f"{mod._BRANCH_NAMESPACE}/{wt_name}"
        wt_path = os.path.join(repo_root, mod._WORKTREES_DIRNAME, wt_name)
        base = _run_git(["rev-parse", "HEAD"], cwd=repo_root)
        base_commit = base.stdout.strip() if base.returncode == 0 else ""
        result = _run_git(
            ["worktree", "add", str(wt_path), "-b", branch, "HEAD"], cwd=repo_root)
        if result.returncode != 0:
            return None
        return {"path": str(wt_path), "branch": branch,
                "repo_root": repo_root, "base_commit": base_commit}

    def mark_worktree_payload_unproven(payload, reason, *, unmeasured="commits/dirty"):
        path = payload.get("path", "")
        branch = payload.get("branch", "")
        payload["inspection_failed"] = True
        payload["note"] = (
            f"git inspection failed ({reason}): {unmeasured} UNKNOWN — not "
            "proven zero/clean. The worktree and branch were preserved "
            f"— inspect {path} (branch {branch}) before assuming no work.")
        return payload

    def unproven_worktree_payload(info, reason):
        return mark_worktree_payload_unproven({
            "path": info.get("path", ""),
            "branch": info.get("branch", ""),
            "commits": 0,
            "dirty": False,
            "pruned": False,
        }, reason)

    def finalize_subagent_worktree(info, prune=True):
        path = info.get("path", "")
        branch = info.get("branch", "")
        repo_root = info.get("repo_root", "")
        base_commit = info.get("base_commit", "")
        payload = {"path": path, "branch": branch, "commits": 0,
                   "dirty": False, "pruned": False}
        import os
        if not path or not os.path.isdir(path):
            payload["pruned"] = True
            return payload

        def _unproven(reason, *, unmeasured="commits/dirty"):
            return mark_worktree_payload_unproven(payload, reason, unmeasured=unmeasured)

        if not base_commit:
            return _unproven("no base_commit recorded — commit count unmeasurable",
                             unmeasured="commits")
        failed = []
        unmeasured = []
        counted = _run_git(["rev-list", "--count", f"{base_commit}..HEAD"], cwd=path)
        if counted.returncode == 0:
            payload["commits"] = int(counted.stdout.strip() or 0)
        else:
            failed.append(f"rev-list exit {counted.returncode}: {counted.stderr.strip()[:200]}")
            unmeasured.append("commits")
        status = _run_git(["status", "--porcelain"], cwd=path)
        if status.returncode == 0:
            payload["dirty"] = bool(status.stdout.strip())
        else:
            failed.append(f"status exit {status.returncode}: {status.stderr.strip()[:200]}")
            unmeasured.append("dirty")
        if failed:
            return _unproven("; ".join(failed), unmeasured="/".join(unmeasured))
        if prune and payload["commits"] == 0 and not payload["dirty"]:
            removed = _run_git(["worktree", "remove", "--force", path],
                               cwd=repo_root or path)
            if removed.returncode == 0:
                _run_git(["branch", "-D", branch], cwd=repo_root or path)
                payload["pruned"] = True
        return payload

    def build_worktree_context_note(info):
        return (
            "\n\n[WORKTREE ISOLATION] You are working in an isolated git worktree "
            f"at: {info.get('path')}\n"
            f"Your dedicated branch is: {info.get('branch')}\n"
            "All file edits and shell commands must happen inside this worktree "
            "directory (your terminal already starts there). Do NOT cd to the "
            "main repository checkout. Commit your changes to your branch when "
            "done; the parent agent will review and merge your branch. If you "
            "make no commits and leave the tree clean, the worktree is discarded "
            "automatically.")

    mod.resolve_repo_root = resolve_repo_root
    mod.create_subagent_worktree = create_subagent_worktree
    mod.mark_worktree_payload_unproven = mark_worktree_payload_unproven
    mod.unproven_worktree_payload = unproven_worktree_payload
    mod.finalize_subagent_worktree = finalize_subagent_worktree
    mod.build_worktree_context_note = build_worktree_context_note
    return mod


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        git = ScriptedGit(c.get("git"))
        mod = make_module(git)
        if kind == "resolve_repo_root":
            out = mod.resolve_repo_root(c.get("path"))
        elif kind == "create":
            out = mod.create_subagent_worktree(c.get("cwd"), c.get("id"))
        elif kind == "mark_unproven":
            u = c.get("unmeasured")
            kw = {"unmeasured": u} if u is not None else {}
            out = mod.mark_worktree_payload_unproven(dict(c.get("payload", {})),
                                                     c.get("reason", ""), **kw)
        elif kind == "unproven_payload":
            out = mod.unproven_worktree_payload(c.get("info", {}), c.get("reason", ""))
        elif kind == "finalize":
            # finalize probes isdir(path): fixture says whether to pretend it exists
            import os
            pth = (c.get("info") or {}).get("path", "")
            exists = c.get("exists", False)
            real = os.path.isdir(pth)
            if exists != real:
                # point the probe at a directory with matching existence
                alt = "/tmp" if exists else "/nonexistent_swt_probe"
                info = dict(c.get("info") or {})
                info["path"] = alt if pth else pth
                out = mod.finalize_subagent_worktree(info, prune=bool(c.get("prune", True)))
            else:
                out = mod.finalize_subagent_worktree(dict(c.get("info") or {}),
                                                     prune=bool(c.get("prune", True)))
        elif kind == "context_note":
            out = mod.build_worktree_context_note(c.get("info", {}))
        else:
            out = None
        print(json.dumps({"out": out}, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
