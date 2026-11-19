"""watercup_paths — single cross-OS path resolver for the watercup fleet.

Every watercup component (twins, runner, driver, generators) refers to THESE
rules instead of inlining box-specific paths. Resolution order for the live
quarry (the Hermes Python tree the C ports are verified against):

  1. WATERCUP_QUARRY env var (explicit override, any OS)
  2. <repo>/.hermes/quarry-path or ~/.hermes/quarry-path (one-line file)
  3. git config slermes.quarry (repo-local, then global)
  4. sibling directory 'hermes-agent' containing agent/ + gateway/
  5. any sibling of the repo containing agent/ + gateway/ (name-agnostic)
  6. the repo's parent itself, if it contains agent/ (legacy Linux layout)

No absolute paths are baked into generated artifacts; a gate tree copied to
another machine resolves its own quarry by these rules at run time.
"""
import os
import subprocess
import sys
from pathlib import Path

_HERE = Path(__file__).resolve().parent          # <repo>/tests/watercup
REPO = _HERE.parent.parent                       # <repo>


def _is_quarry(p: Path) -> bool:
    return p.is_dir() and (p / "agent").is_dir() and (p / "gateway").is_dir()


def _from_config_file() -> Path | None:
    for base in (REPO / ".hermes", Path.home() / ".hermes"):
        f = base / "quarry-path"
        try:
            if f.is_file():
                line = f.read_text(encoding="utf-8").strip().splitlines()[0].strip()
                p = Path(os.path.expanduser(line))
                if _is_quarry(p):
                    return p
        except Exception:
            pass
    return None


def _from_git_config() -> Path | None:
    for scope in ("--local", "--global"):
        try:
            out = subprocess.run(
                ["git", "config", scope, "slermes.quarry"],
                cwd=str(REPO), capture_output=True, text=True, timeout=10,
            ).stdout.strip()
            if out:
                p = Path(os.path.expanduser(out))
                if _is_quarry(p):
                    return p
        except Exception:
            pass
    return None


def quarry_root() -> Path:
    env = os.environ.get("WATERCUP_QUARRY")
    if env:
        p = Path(os.path.expanduser(env))
        if _is_quarry(p):
            return p
    for finder in (_from_config_file, _from_git_config):
        p = finder()
        if p:
            return p
    parent = REPO.parent
    sib = parent / "hermes-agent"
    if _is_quarry(sib):
        return sib
    try:
        for p in sorted(parent.iterdir()):
            if p != REPO and _is_quarry(p):
                return p
    except OSError:
        pass
    if _is_quarry(parent):
        return parent  # legacy: quarry packages sat directly in the parent
    raise RuntimeError(
        "watercup: cannot locate the live quarry (need a tree with agent/ + "
        "gateway/). Set WATERCUP_QUARRY, write .hermes/quarry-path, or "
        "git config slermes.quarry <path>."
    )


def ensure_sys_path() -> Path:
    """Put the quarry and this module's dir on sys.path (idempotent)."""
    q = quarry_root()
    for entry in (str(q), str(_HERE)):
        if entry not in sys.path:
            sys.path.insert(0, entry)
    return q


def posix(p) -> str:
    return Path(p).as_posix()
