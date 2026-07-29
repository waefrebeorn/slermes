#!/usr/bin/env python3
"""
sta_oracle_coding_context.py — oracle for t_port_coding_context.c.

Recomputes each case from the LIVE agent/coding_context.py and emits one JSON
line per case (same shape as the C harness). The runner diffs them byte-for-byte.
"""
import json
import os
import sys
from pathlib import Path


def _load():
    """Load the live coding_context module from the repo root."""
    repo_root = Path(__file__).resolve().parents[3]
    if str(repo_root) not in sys.path:
        sys.path.insert(0, str(repo_root))
    try:
        import agent.coding_context as cc
        return cc
    except ImportError:
        sys.stderr.write(f"Cannot load agent.coding_context from {repo_root}\n")
        return None


cc = _load()
if cc is None:
    sys.exit(2)


def emit_detect_profile(c):
    mode = c.get("mode", "auto")
    platform = c.get("platform", "")
    cwd = c.get("cwd", ".")
    # Use the internal _detect_profile_name function directly
    out = cc._detect_profile_name(mode, platform, cwd)
    return {"fn": "detect_profile_name", "out": out if out else ""}


def emit_find_git_root(c):
    cwd = c.get("cwd", ".")
    from hermes_cli._subprocess_compat import bounded_git_probe
    try:
        result = bounded_git_probe(["git", "-C", cwd, "rev-parse", "--show-toplevel"], timeout=2.5)
        root = result.strip() if result else ""
        return {"fn": "find_git_root", "found": bool(root), "root": root}
    except Exception:
        return {"fn": "find_git_root", "found": False, "root": ""}


def emit_find_marker_root(c):
    cwd = c.get("cwd", ".")
    resolved = Path(cwd).resolve()
    home = Path.home().resolve()
    temp_root = Path("/tmp").resolve()
    
    _PROJECT_MARKERS = (
        "pyproject.toml", "setup.py", "setup.cfg", "requirements.txt",
        "package.json", "tsconfig.json", "deno.json",
        "Cargo.toml", "go.mod", "pom.xml", "build.gradle", "build.gradle.kts",
        "Gemfile", "composer.json", "mix.exs", "pubspec.yaml",
        "CMakeLists.txt", "Makefile", "Dockerfile",
        "AGENTS.md", "CLAUDE.md", ".cursorrules",
    )
    
    for parent in [resolved, *resolved.parents]:
        if parent == home or parent == temp_root:
            continue
        for marker in _PROJECT_MARKERS:
            if (parent / marker).exists():
                return {"fn": "find_marker_root", "found": True, "root": str(parent)}
    return {"fn": "find_marker_root", "found": False, "root": ""}


def emit_coding_system_blocks(c):
    platform = c.get("platform", "")
    cwd = c.get("cwd", ".")
    model = c.get("model", "")
    blocks = cc.coding_system_blocks(platform=platform, cwd=cwd, model=model)
    return {"fn": "coding_system_blocks", "count": len(blocks), "blocks": blocks}


def emit_build_workspace_block(c):
    cwd = c.get("cwd", ".")
    out = cc.build_coding_workspace_block(cwd)
    return {"fn": "build_workspace_block", "out": out if out else ""}


DISPATCH = {
    "detect_profile_name": emit_detect_profile,
    "find_git_root": emit_find_git_root,
    "find_marker_root": emit_find_marker_root,
    "coding_system_blocks": emit_coding_system_blocks,
    "build_workspace_block": emit_build_workspace_block,
}


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_coding_context.py <cases.json>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        fn = DISPATCH.get(op)
        if fn:
            out = fn(c)
        else:
            out = {"fn": op}
        sys.stdout.write(json.dumps(out, ensure_ascii=False, separators=(",", ":")) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())