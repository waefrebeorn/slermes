#!/usr/bin/env python3
"""Differential oracle for the fill decision layer (src/tools/browser_vault_fill_plan.c).

Two security decisions, both pure, both compared against the real Python:

  origin guard   tools.browser_vault_tool._bound_page_origin with its two page lookups patched to fixed
                 values, so the guard's own logic runs: no bound origin, undeterminable page, exact
                 match (several origins for a manager item), and the mismatch refusal with both sides
                 named. Matching is exact -- a subdomain or a parent domain must NOT pass.
  identifier     tools.browser_vault_tool._identifier_fill over a control grid: the same form wins,
                 then higher score, then lower index; an identifier-only step with no password field
                 is the whole fill; an empty identifier fills nothing.

Usage: python tests/oracle/vault_fill_plan_oracle.py [--keep]
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

QUARRY = Path(os.environ.get("HERMES_QUARRY", r"C:/Users/eman5/hermes-agent"))
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(QUARRY))

if not (os.environ.get("USERPROFILE") or os.environ.get("HOME")):
    print("SKIPPED: neither USERPROFILE nor HOME is set, so the quarry cannot resolve its home dir")
    raise SystemExit(3)

import tools.browser_vault_tool as bvt                                  # noqa: E402
from agent.vault_login_classifier import ClassifiedLoginControl, LoginControl  # noqa: E402


@dataclass
class FakeMeta:
    id: str
    kind: str
    origin: str | None
    allowed_origins: tuple = ()


def build_exe() -> Path:
    exe = REPO / ".testbin" / "vault_fill_plan_diff.exe"
    if exe.exists():
        return exe
    build = subprocess.run(
        ["gcc", "-O1", "-w", "-I", "include", "-I", "lib", "-I", "lib/libjson",
         "src/tools/browser_vault_fill_plan.c", "tests/unit/vault_fill_plan_diff.c",
         "lib/libjson/json.o", "-o", str(exe)],
        cwd=str(REPO), capture_output=True, text=True)
    if build.returncode != 0:
        print("cannot build the harness:")
        print(build.stderr[-2500:])
        raise SystemExit(2)
    return exe


def guard_case(exe: Path, meta: FakeMeta, page_origin: str | None):
    """Runs upstream's _bound_page_origin with the page lookups pinned, and the C guard, on one input."""
    saved = bvt.saved_origins(meta)
    origins_path = REPO / ".testbin" / "fp_origins.json"
    origins_path.write_text(json.dumps(saved), encoding="utf-8")

    orig_focus, orig_current = bvt._focus_bound_origin, bvt._current_page_origin
    # No tab is focused by the guard when the page origin is already one of the bound ones; pinning both
    # keeps "which tab does the supervisor hold" out of a decision about origins.
    bvt._focus_bound_origin = lambda task_id, origin, kind: None
    bvt._current_page_origin = lambda task_id: page_origin
    try:
        want = bvt._bound_page_origin("t1", meta, meta.kind)
    finally:
        bvt._focus_bound_origin, bvt._current_page_origin = orig_focus, orig_current

    have = json.loads(subprocess.run(
        [str(exe), "guard", str(origins_path), page_origin or "-", meta.id, meta.kind],
        capture_output=True, text=True).stdout)
    return want, have


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    exe = build_exe()
    agree = failed = 0

    def check(name: str, want, have) -> None:
        nonlocal agree, failed
        if want == have:
            agree += 1
        else:
            failed += 1
            print(f"FAIL {name}")
            print(f"   want: {want!r}")
            print(f"   got:  {have!r}")

    # ── origin guard ─────────────────────────────────────────────────────────
    login = FakeMeta("vault_abc", "login", "https://mail.example.com")
    multi = FakeMeta("vault_mgr", "login", "https://a.example.com",
                     allowed_origins=("https://a.example.com", "https://b.example.com"))
    no_origin = FakeMeta("vault_none", "address", None)

    cases = [
        ("exact", login, "https://mail.example.com"),
        ("mismatch", login, "https://evil.example.com"),
        ("subdomain", login, "https://sub.mail.example.com"),
        ("parent", login, "https://example.com"),
        ("scheme", login, "http://mail.example.com"),
        ("port", login, "https://mail.example.com:8443"),
        ("unknown-page", login, None),
        ("multi-first", multi, "https://a.example.com"),
        ("multi-second", multi, "https://b.example.com"),
        ("multi-other", multi, "https://c.example.com"),
        ("no-origin", no_origin, "https://shop.example.com"),
    ]
    for name, meta, page in cases:
        want, have = guard_case(exe, meta, page)
        check(f"guard[{name}]", want, have)

    # ── identifier pick ──────────────────────────────────────────────────────
    def ctl(index, token, score, form_index):
        return ClassifiedLoginControl(control=LoginControl(index=index, form_index=form_index,
                                                           autocomplete=token, name="", label="",
                                                           type="text"),
                                      score=score, token=token)

    grids = [
        ("same-form-wins", [ctl(0, "email", 5, 0), ctl(3, "username", 9, 1),
                            ctl(5, "username", 7, 0)], [{"index": 5, "token": "current-password"}]),
        ("higher-score", [ctl(1, "username", 4, 0), ctl(2, "email", 8, 0)],
         [{"index": 1, "token": "current-password"}]),
        ("lower-index-tie", [ctl(7, "username", 5, 0), ctl(4, "email", 5, 0)],
         [{"index": 7, "token": "current-password"}]),
        ("identifier-only-step", [ctl(2, "email", 6, 0)], []),
        ("no-identifier-control", [ctl(0, "current-password", 5, 0)], [{"index": 0, "token": "current-password"}]),
        ("empty", [], []),
    ]
    for name, classified, pw in grids:
        cl_path = REPO / ".testbin" / "fp_classified.json"
        pw_path = REPO / ".testbin" / "fp_pw.json"
        cl_path.write_text(json.dumps([
            {"index": c.control.index, "token": c.token, "score": c.score,
             "form_index": c.control.form_index} for c in classified]), encoding="utf-8")
        pw_path.write_text(json.dumps(pw), encoding="utf-8")
        want = bvt._identifier_fill(classified, pw, "ada@example.com")
        have = json.loads(subprocess.run(
            [str(exe), "ident", str(cl_path), str(pw_path), "ada@example.com"],
            capture_output=True, text=True).stdout)
        check(f"ident[{name}]", want, have)

    # empty identifier fills nothing, in both
    cl_path = REPO / ".testbin" / "fp_classified.json"
    cl_path.write_text(json.dumps([{"index": 0, "token": "email", "score": 9, "form_index": 0}]),
                       encoding="utf-8")
    pw_path = REPO / ".testbin" / "fp_pw.json"
    pw_path.write_text("[]", encoding="utf-8")
    check("ident[empty-identifier]", bvt._identifier_fill([ctl(0, "email", 9, 0)], [], ""),
          json.loads(subprocess.run([str(exe), "ident", str(cl_path), str(pw_path), "-"],
                                    capture_output=True, text=True).stdout))

    print(f"vault-fill-plan oracle: {agree} agree, {failed} problem(s)")
    if not args.keep:
        for name in ("fp_origins.json", "fp_classified.json", "fp_pw.json"):
            (REPO / ".testbin" / name).unlink(missing_ok=True)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
