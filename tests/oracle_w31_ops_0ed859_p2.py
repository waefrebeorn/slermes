#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/ops.py — gate w31_ops_0ed859_p2
(ops payload / argv derivation).

The live router module is imported BY FILE PATH (FastAPI + the pydantic models
are seams; this host also refuses pydantic's native extension).  Pure helpers
are called directly:

  * ``_pool_entry_summary`` over a PooledCredential-shaped record (a bare
    attribute bag stands in for the dataclass; ``__getattr__`` yields None for a
    field the fixture omits, which is what the C side emits as null);
  * ``_safe_backup_upload_name`` over an uploaded filename;
  * ``_hook_body_fields`` over a hook body.

Four derivations live inline in their routes and are transcribed verbatim:
``create_hook``'s VALID_HOOKS guard
(``event not in valid_hooks`` -> f"Unknown event '{event}'. Valid: {',
'.join(sorted(valid_hooks))}"), ``_spawn_import``'s argv, ``reset_memory``'s
target validation + deletion plan, and ``approve_pairing``'s request-id/code
dispatch (which includes the real ``PairingStore.looks_like_request_id``).

Seams, all supplied as data: the credential pool on disk, the filesystem
(``Path.exists``/stat arrives as ``memory_exists``/``user_exists`` flags), the
hook consent allowlist, config.yaml, and every subprocess spawn.  A refusal is
the ``HTTPException`` the route would have raised, projected as
``{"ok": false, "status": int, "detail": str}``.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_ops_0ed859_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

if os.name == "nt" and not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)

sys.path.append(str(Path(__file__).resolve().parent))  # stub helper only; must NOT shadow the quarry packages

from hermes_seam_stubs import install as _install_seams  # noqa: E402

HTTPException = _install_seams()


def _load(name, rel):
    spec = importlib.util.spec_from_file_location(name, QUARRY / rel)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


live = _load("w31_live_ops_p2", "hermes_cli/web_routers/ops.py")
from gateway.pairing import PairingStore  # noqa: E402  (real request-id shape check)

WANTED = {
    "pool_entry_summary", "safe_backup_upload_name", "hook_body_fields",
    "hook_event_verdict", "spawn_import_argv", "memory_reset_target",
    "pairing_approve_target", "backup_download_verdict",
}

MEMORY_FILES = (("MEMORY.md", "memory"), ("USER.md", "user"))


class Entry:
    """PooledCredential stand-in: attributes from the fixture, None when absent."""

    def __init__(self, d):
        self.__dict__.update(d if isinstance(d, dict) else {})

    def __getattr__(self, key):
        return None


class Body:
    """Hook body stand-in (HookCreate / HookDelete)."""

    def __init__(self, d):
        self.__dict__.update(d)


def ok(**extra):
    out = {"ok": True, "status": 200, "detail": None}
    out.update(extra)
    return out


def refused(status, detail):
    return {"ok": False, "status": status, "detail": detail}


def pool_entry_summary(r):
    return live._pool_entry_summary(Entry(r.get("entry")), r.get("index", 1))


def safe_backup_upload_name(r):
    return live._safe_backup_upload_name(r.get("filename"))


def hook_body_fields(r):
    try:
        event, command = live._hook_body_fields(
            Body({"event": r.get("event"), "command": r.get("command")}))
    except HTTPException as exc:
        return refused(exc.status_code, exc.detail)
    return ok(event=event, command=command)


def hook_event_verdict(r):
    # create_hook, verbatim: the body fields first, then the VALID_HOOKS guard.
    verdict = hook_body_fields(r)
    if not verdict["ok"]:
        return verdict
    valid_hooks = r.get("valid_hooks")
    if valid_hooks is not None and verdict["event"] not in valid_hooks:
        return refused(
            400,
            "Unknown event '%s'. Valid: %s"
            % (verdict["event"], ", ".join(sorted(valid_hooks))),
        )
    return verdict


def spawn_import_argv(r):
    # _spawn_import, verbatim.
    args = ["import", r.get("archive") or ""]
    force = bool(r.get("force"))
    keep_host = bool(r.get("keep_host"))
    messaging_off = bool(r.get("messaging_off"))
    if force or keep_host:
        args.append("--force")
    if keep_host:
        args.append("--keep-host")
    if keep_host and messaging_off:
        args.append("--messaging-off")
    return args


def memory_reset_target(r):
    # reset_memory, verbatim (Path.exists() is the fixture flag).
    target = (r.get("target") or "all").strip().lower()
    if target not in {"all", "memory", "user"}:
        return refused(400, "target must be all, memory, or user")
    exists = {"MEMORY.md": bool(r.get("memory_exists")), "USER.md": bool(r.get("user_exists"))}
    deleted = [fname for fname, key in MEMORY_FILES if target in {"all", key} and exists[fname]]
    return ok(target=target, deleted=deleted)


def pairing_approve_target(r):
    # approve_pairing, verbatim.
    platform = (r.get("platform") or "").lower().strip()
    target = (r.get("request_id") or r.get("code") or "").strip()
    if not platform or not target:
        return refused(400, "platform and request_id or code are required")
    by_request_id = bool(r.get("request_id")) or PairingStore.looks_like_request_id(target)
    return ok(
        platform=platform,
        target=target,
        by_request_id=by_request_id,
        approve_arg=target if by_request_id else target.upper(),
    )


def backup_download_verdict(r):
    # _dashboard_backup_path, verbatim: resolve(strict=True) is the fixture's
    # ``resolved`` answer, and containment uses the live _path_is_under.
    resolve_error = r.get("resolve_error") or ""
    resolved = r.get("resolved")
    if resolve_error == "not_found" or not resolved:
        return refused(404, "Backup not found")
    if resolve_error == "invalid":
        return refused(400, "Invalid backup path")
    if not live._path_is_under(Path(r.get("backup_dir") or ""), Path(resolved)):
        return refused(403, "Backup is outside the dashboard backup directory")
    if not r.get("is_file"):
        return refused(404, "Backup not found")
    return ok()


def expected(r):
    f = r.get("func")
    if f == "pool_entry_summary":
        return pool_entry_summary(r)
    if f == "safe_backup_upload_name":
        return safe_backup_upload_name(r)
    if f == "hook_body_fields":
        return hook_body_fields(r)
    if f == "hook_event_verdict":
        return hook_event_verdict(r)
    if f == "spawn_import_argv":
        return spawn_import_argv(r)
    if f == "memory_reset_target":
        return memory_reset_target(r)
    if f == "pairing_approve_target":
        return pairing_approve_target(r)
    if f == "backup_download_verdict":
        return backup_download_verdict(r)
    raise AssertionError("unknown func %r" % (f,))


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
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
