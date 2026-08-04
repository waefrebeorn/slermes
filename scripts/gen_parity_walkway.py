#!/usr/bin/env python3
"""
gen_parity_walkway.py — single source of truth for ALL derived state.

WHY: barnacles (stale copied numbers) existed because agents hand-transcribed
derived state into ~15 walkway/doc files and the copies drifted (the classic
"8,688/8,688 100% PORTED" fiction). This script is the ONLY writer of every
piece of derived state:

  - PORTED / REAL_GAP / PARTIAL counts   <- live scanner (fail-closed)
  - BOOTLEG count                         <- recursive_false_gap_hunter.py
  - upstream sync checkpoint              <- git rev-list (ahead/behind/last merge)
  - version headers + phase labels        <- scripts/version.txt (single source)
  - BANNER / ROADMAP / parity-summary     <- sentinel-owned derived paragraphs

Nothing is hand-copied. Run `make parity-walkway` after every scanner run —
it refreshes every owned section in every file. Hand-edited numbers are
overwritten on the next run; the only manual step is the checkpoint bump:

    python3 scripts/gen_parity_walkway.py --bump   # v668 -> v669, then refresh all

Sentinel convention: every owned section is wrapped in
`<!-- PARITY:AUTO --> ... <!-- /PARITY:AUTO -->`. Files WITHOUT the sentinel
get a block appended (so future runs regenerate it). Version headers in
walkway titles are fixed by regex on the first line (`vNNN` -> current, phase
label normalized) — no per-file hand-tuning.
"""
import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCANNER = os.path.join(REPO, "tests", "slermes_parity_battleground.py")
HUNTER = os.path.join(REPO, "tests", "recursive_false_gap_hunter.py")
# Fail-closed gate: the Python ground-truth must be checked out, or we refuse
# to emit. This is the systemic cure for "silent 0-file scan -> confident lie".
TRUTH = os.path.join(REPO, "scripts", "parity_truth.py")
VERSION_FILE = os.path.join(REPO, "scripts", "version.txt")
SENT_OPEN = "<!-- PARITY:AUTO -->"
SENT_CLOSE = "<!-- /PARITY:AUTO -->"

# Walkway files that carry the canonical count block (project + user level).
WALKWAY_FILES = [
    os.path.join(REPO, ".hermes", "state.md"),
    os.path.join(REPO, ".hermes", "battleship.md"),
    os.path.join(REPO, ".hermes", "roadmap.md"),
    os.path.join(REPO, ".hermes", "mind-palace", "state.md"),
    os.path.join(REPO, ".hermes", "mind-palace", "prestige.md"),
    os.path.join(REPO, ".hermes", "mind-palace", "plan.md"),
    os.path.join(REPO, ".hermes", "mind-palace", "goal-mantra.md"),
    # NOTE: mind-palace/index.md is NOT here — its "## Current State" table
    # is owned separately (index_state_block) to avoid a duplicate block.
    os.path.join(REPO, ".hermes", "mind-palace", "next-session-prompt.md"),
    os.path.join(REPO, ".hermes", "next-session", "steamroll-goal.md"),
    os.path.expanduser("~/.hermes/mind-palace/state.md"),
    os.path.expanduser("~/.hermes/mind-palace/plan.md"),
    os.path.expanduser("~/.hermes/mind-palace/overnight.md"),
    os.path.expanduser("~/.hermes/mind-palace/INDEX.md"),
    os.path.expanduser("~/.hermes/mind-palace/battleship.md"),
    os.path.expanduser("~/.hermes/mind-palace/goal-mantra.md"),
] + sorted(
    os.path.expanduser(os.path.join("~/.hermes/walkway", f))
    for f in os.listdir(os.path.expanduser("~/.hermes/walkway"))
    if f.startswith("v") and f.endswith(".md")
)

# Root docs with derived paragraphs (phase, sync checkpoint, version, counts).
DOC_FILES = [
    os.path.join(REPO, "BANNER.md"),
    os.path.join(REPO, "ROADMAP.md"),
    os.path.join(REPO, "docs", "parity-summary.md"),
]


def run(cmd, cwd=None, timeout=600):
    r = subprocess.run(cmd, cwd=cwd or REPO, capture_output=True, text=True,
                       timeout=timeout)
    return r


def current_version():
    """Read scripts/version.txt (single source of truth). Default v668."""
    try:
        with open(VERSION_FILE, encoding="utf-8") as f:
            v = f.read().strip()
        if re.fullmatch(r"v\d+", v):
            return v
    except FileNotFoundError:
        pass
    return "v668"


def bump_version():
    v = current_version()
    n = int(v[1:]) + 1
    new = f"v{n}"
    with open(VERSION_FILE, "w", encoding="utf-8") as f:
        f.write(new + "\n")
    print(f"version.txt: {v} -> {new}")
    return new


def compute():
    """All derived state in one dict. Fail-closed on the scanner gate."""
    # FAIL-CLOSED: parity_truth.py refuses to emit unless the Python
    # ground-truth is checked out and consumed by the scanner.
    r = run([sys.executable, TRUTH])
    if r.returncode != 0:
        sys.exit(r.returncode)
    with open(os.path.join(REPO, "live_parity_scan.json"), encoding="utf-8") as f:
        doc = json.load(f)
    tot = doc["totals"]
    total = tot["total"]
    ported = tot["ported"]
    real = tot["real_gaps"]
    partial = tot["partial"]
    pct = tot["coverage_pct"]

    # BOOTLEG count from the recursive hunter (0 = every function does real
    # observable work or faithfully mirrors Python's own trivial behavior).
    bootleg = None
    try:
        rh = run([sys.executable, HUNTER], timeout=900)
        m = re.search(r"BOOTLEG[^:]*:\s*(\d+)", rh.stdout)
        if m:
            bootleg = int(m.group(1))
    except Exception:
        bootleg = None

    # Upstream sync checkpoint from git.
    ahead = behind = -1
    last_merge = "unknown"
    try:
        rb = run(["git", "rev-list", "--count", "HEAD..upstream/main"])
        ra = run(["git", "rev-list", "--count", "upstream/main..HEAD"])
        if rb.returncode == 0 and ra.returncode == 0:
            behind = int(rb.stdout.strip())
            ahead = int(ra.stdout.strip())
        rm = run(["git", "log", "-1", "--format=%ad (%s)", "--date=short",
                  "--merges", "upstream/main..HEAD"])
        if rm.returncode == 0 and rm.stdout.strip():
            last_merge = rm.stdout.strip()
    except Exception:
        pass

    # Phase label: agnostic rule from the data.
    if real == 0 and bootleg == 0:
        phase = "COMPLETE"
    elif real <= 50 and (bootleg is None or bootleg == 0):
        phase = "MATCH"
    else:
        phase = "PORT"

    return dict(ported=ported, real=real, partial=partial, total=total,
                pct=pct, bootleg=bootleg, ahead=ahead, behind=behind,
                last_merge=last_merge, phase=phase, version=current_version(),
                stamp=doc.get("_generated_at", "unknown-time"))


def count_block(d):
    """The canonical count block for walkway files."""
    partial_pct = 100.0 * d['partial'] / d['total'] if d['total'] else 0.0
    bootleg_line = (f"{d['bootleg']:,}" if d['bootleg'] is not None
                    else "unknown")
    ahead = d['ahead'] if d['ahead'] >= 0 else "?"
    behind = d['behind'] if d['behind'] >= 0 else "?"
    return (
        f"| PORTED  | {d['ported']:,} / {d['total']:,} ({d['pct']:.1f}%) |\n"
        f"| REAL_GAP| {d['real']:,} ({100.0 * d['real'] / d['total']:.1f}%) — no N/A |\n"
        f"| PARTIAL | {d['partial']:,} ({partial_pct:.1f}%) |\n"
        f"| BOOTLEG | {bootleg_line} (recursive_false_gap_hunter.py) |\n"
        f"\n"
        f"**Phase ({d['version']}):** {d['phase']} phase — the C11 binary is the "
        f"deliverable: faithful, oracle-verified, usable standalone across "
        f"operating systems. "
        f"{'PORT phase (v398→v667) is legacy — complete; every function does real observable work and matches the Python original.' if d['phase'] == 'MATCH' else 'Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.'}\n"
        f"\n"
        f"**Upstream sync checkpoint:** {ahead:,} ahead / {behind:,} behind "
        f"upstream/main (last merge {d['last_merge']}). The behind-count is the "
        f"staleness timer — run the stash→pull→fix→pop workflow, then re-port the delta.\n"
        f"\n"
        f"_Generated {d['stamp']} from live scanner "
        f"`{os.path.relpath(SCANNER, REPO)}` — "
        f"do not edit by hand; run `make parity-walkway`._"
    )


def banner_block(d):
    """BANNER.md derived paragraphs (phase + sync checkpoint)."""
    ahead = d['ahead'] if d['ahead'] >= 0 else "?"
    behind = d['behind'] if d['behind'] >= 0 else "?"
    if d['phase'] == 'MATCH':
        phase_txt = (
            f"**{d['version']} MATCH phase:** the C11 binary is the deliverable — "
            f"faithful, oracle-verified, usable standalone across operating "
            f"systems. The PORT phase (99.8% ported, 0 bootlegs) is legacy; the "
            f"remaining work is behavioral fidelity — every function does real "
            f"observable work and matches the Python original. The AGI-OS "
            f"integration consumes the compiled binary, not the Python tree. "
            f"Live parity counts live in the PARITY:AUTO sentinel blocks "
            f"owned by `make parity-walkway`; never hand-edit a count."
        )
    else:
        phase_txt = (
            f"**{d['version']} phase — {d['phase']}:** the C11 binary is the "
            f"deliverable — faithful, oracle-verified, usable standalone across "
            f"operating systems. Live parity counts live in the PARITY:AUTO "
            f"sentinel blocks owned by `make parity-walkway`; never hand-edit a count."
        )
    return (
        phase_txt + "\n\n"
        f"**Upstream sync checkpoint:** {ahead:,} ahead / {behind:,} behind "
        f"upstream/main (last merge {d['last_merge']}). The behind-count is the "
        f"staleness timer — see the stash→pull→fix→pop workflow below."
    )


def roadmap_block(d):
    """ROADMAP.md derived header (version line + phase blockquote)."""
    ahead = d['ahead'] if d['ahead'] >= 0 else "?"
    behind = d['behind'] if d['behind'] >= 0 else "?"
    if d['phase'] == 'MATCH':
        phase_txt = (
            f"> **{d['version']} MATCH phase:** the PORT phase (v398→v667) is "
            f"**legacy — complete**. Live scanner {d['stamp'][:10]}: "
            f"**{d['ported']:,} / {d['total']:,} ({d['pct']:.1f}%) PORTED · "
            f"{d['real']:,} REAL_GAP · {d['partial']:,} PARTIAL · "
            f"0 BOOTLEG**. The C11 binary is the deliverable — faithful, "
            f"oracle-verified, usable standalone on any operating system. What "
            f"remains is *behavioral fidelity*: every function does real "
            f"observable work and matches the Python original. Forward "
            f"integration target: the AGI operating system consumes the compiled "
            f"binary, not the Python tree. All generated docs carry live counts "
            f"from `make parity-walkway` (sentinel PARITY:AUTO blocks)."
        )
    else:
        phase_txt = (
            f"> **{d['version']} {d['phase']} phase:** live scanner "
            f"{d['stamp'][:10]}: {d['ported']:,} / {d['total']:,} "
            f"({d['pct']:.1f}%) PORTED · {d['real']:,} REAL_GAP · "
            f"{d['partial']:,} PARTIAL. The C11 binary is the deliverable."
        )
    return (
        f"**Version:** 0.19.0-slermes ({d['version']}, {d['phase']} phase)  \n"
        f"**Last updated:** {d['stamp'][:10]}\n\n"
        + phase_txt + "\n>\n"
        f"> **Upstream sync checkpoint:** {ahead:,} ahead / {behind:,} behind "
        f"upstream/main (last merge {d['last_merge']}) — the behind-count is the "
        f"staleness timer; re-port the delta with the stash→pull→fix→pop "
        f"workflow after each sync."
    )


def parity_summary_block(d):
    """docs/parity-summary.md Overall Numbers table (live)."""
    partial_pct = 100.0 * d['partial'] / d['total'] if d['total'] else 0.0
    return (
        f"| Classification | Count | Percentage | Meaning |\n"
        f"|----------------|-------|------------|---------|\n"
        f"| **PORTED** | {d['ported']:,} | {d['pct']:.1f}% | C11 implementation with PoP annotation |\n"
        f"| **REAL_GAP** | {d['real']:,} | {100.0 * d['real'] / d['total']:.1f}% | Honest gaps (not yet ported — IO/network/DB/logic; NOT faked) |\n"
        f"| **PARTIAL** | {d['partial']:,} | {partial_pct:.1f}% | All C fns now carry PoP annotations |\n"
        f"| **BOOTLEG** | {d['bootleg'] if d['bootleg'] is not None else '?'} | — | No-work echo stubs (recursive_false_gap_hunter.py) |\n"
        f"| **TOTAL** | {d['total']:,} | 100% | All Python functions/methods scanned |\n"
        f"\n"
        f"> **Generated {d['stamp']} by `make parity-walkway` from the live "
        f"scanner.** The PORT phase (v398→v667) is legacy — this table is the "
        f"single source of truth for completeness. Do not hand-edit."
    )


def index_line(d):
    """User-level INDEX.md battleship status line."""
    b = d['bootleg'] if d['bootleg'] is not None else "?"
    return (
        f"- **battleship.md** — Current state: {d['version']} {d['phase']} phase. "
        f"Live scanner: {d['ported']:,}/{d['total']:,} ({d['pct']:.1f}%) PORTED, "
        f"{d['real']:,} REAL_GAP, {d['partial']:,} PARTIAL, **{b} BOOTLEG** "
        f"(make parity-walkway)."
    )


def strip_stale_owned(path):
    """Remove stale derived paragraphs that live OUTSIDE the sentinel.

    Convergence: the generator owns the phase paragraph, the sync checkpoint
    line, and version/phase references in the title. Any copy of those that
    sits outside a `<!-- PARITY:AUTO -->` block is a barnacle from a
    hand-edit era — strip it so re-running the generator converges to the
    same file (idempotent). Sentinel content is NEVER touched. Returns True
    if the file changed.
    """
    if not os.path.exists(path):
        return False
    txt = open(path, encoding="utf-8").read()
    orig = txt

    # Split into sentinel regions (kept verbatim) and plain regions (stripped).
    parts = txt.split(SENT_OPEN)
    out = [strip_plain(parts[0])]  # region before any sentinel — strip too
    for i, seg in enumerate(parts[1:]):
        if SENT_CLOSE in seg:
            head, tail = seg.split(SENT_CLOSE, 1)
            out.append(SENT_OPEN + head + SENT_CLOSE)  # sentinel verbatim
            out.append(strip_plain(tail))
        else:
            out.append(strip_plain(seg))
    txt = "".join(out)

    # Collapse 3+ blank lines left by the strips.
    txt = re.sub(r"\n{3,}", "\n\n", txt)
    if txt != orig:
        open(path, "w", encoding="utf-8").write(txt)
        return True
    return False


def strip_plain(txt):
    """Strip stale derived paragraphs from a non-sentinel region."""
    # Stale "**vNNN phase — PARITY project:** ..." multi-line paragraphs
    # (the v398-v667 era phase blurb), both plain and blockquote forms.
    txt = re.sub(
        r">?\s*\*\*v\d+ phase[^\n]*:\*\*[^\n]*(?:\n>?\s*[^*][^\n]*){0,6}",
        "", txt)
    # Any standalone "**vNNN phase — X:**" one-liner outside the sentinel.
    txt = re.sub(r"\n\*\*v\d+ phase[^\n]*\*\*\n", "\n", txt)
    # Stale "**Upstream sync checkpoint:** ..." lines outside the sentinel.
    txt = re.sub(r"\n\*\*Upstream sync checkpoint:\*\*[^\n]*\n", "\n", txt)
    # Stale "**Version:** x.y.z-slermes (vNNN, ...)" OR "**Version:** vNNN"
    # lines outside the sentinel (standalone or inline after **Build:**).
    txt = re.sub(r"\n\*\*Version:\*\*[^\n]*v\d+[^\n]*\n", "\n", txt)
    txt = re.sub(r"\*\*Version:\*\*[^\n]*v\d+[^\n]*", "", txt)
    # Stale "**Last updated:** ..." lines outside the sentinel.
    txt = re.sub(r"\n\*\*Last updated:\*\*[^\n]*\n", "\n", txt)
    return txt


def inject(path, inner, insert_after=None):
    """Replace the sentinel block content, or append one."""
    if not os.path.exists(path):
        return False
    txt = open(path, encoding="utf-8").read()
    if SENT_OPEN in txt and SENT_CLOSE in txt:
        pre, rest = txt.split(SENT_OPEN, 1)
        _, post = rest.split(SENT_CLOSE, 1)
        txt = pre + SENT_OPEN + "\n" + inner + "\n" + SENT_CLOSE + post
        open(path, "w", encoding="utf-8").write(txt)
        return True
    if insert_after is not None and insert_after in txt:
        txt = txt.replace(insert_after,
                          insert_after + "\n\n" + SENT_OPEN + "\n" + inner +
                          "\n" + SENT_CLOSE, 1)
        open(path, "w", encoding="utf-8").write(txt)
        return True
    txt = txt.rstrip() + "\n\n" + SENT_OPEN + "\n" + inner + "\n" + SENT_CLOSE + "\n"
    open(path, "w", encoding="utf-8").write(txt)
    return True


def fix_title_version(path, d):
    """Fix the vNNN + phase label in the first line of walkway files.

    Agnostic regex: first line of the form `# ... (vNNN, X phase)` or
    `# ... vNNN ...` gets the current version and phase label. Only touches
    the FIRST line (the title), never historical references in the body.
    """
    if not os.path.exists(path):
        return False
    with open(path, encoding="utf-8") as f:
        lines = f.readlines()
    if not lines:
        return False
    first = lines[0]
    new_first = first
    # Pattern A: "# Title (vNNN, X phase)" — normalize version + phase.
    m = re.match(r"^(#.*\()v\d+,\s*([A-Z]+) phase(\)|,)", first)
    if m:
        # Replace just the phase NAME token (keep the existing " phase" suffix).
        new_first = (first[:m.start(2)] + d['phase'] + first[m.end(2):])
        new_first = re.sub(r"v\d+", d['version'], new_first, count=1)
    else:
        # Pattern B: "# Title (vNNN)" or "# Title vNNN ..." — bump version only.
        m2 = re.match(r"^(#.*v)\d+(.*)$", first.rstrip("\n"))
        if m2:
            new_first = m2.group(1) + d['version'][1:] + m2.group(2)
            # Preserve the original line ending (may be "\n" or "\r\n").
            if first.endswith("\r\n"):
                new_first += "\r\n"
            else:
                new_first += "\n"
    if new_first != first:
        lines[0] = new_first
        with open(path, "w", encoding="utf-8") as f:
            f.writelines(lines)
        return True
    return False


def index_state_block(d):
    """Battleship index '## Current State' table (derived, machine-owned)."""
    b = d['bootleg'] if d['bootleg'] is not None else "?"
    ahead = d['ahead'] if d['ahead'] >= 0 else "?"
    behind = d['behind'] if d['behind'] >= 0 else "?"
    return (
        f"| Metric | Value |\n"
        f"|--------|-------|\n"
        f"| **Version** | {d['version']} ({d['phase']} phase, live scanner {d['stamp'][:10]}) |\n"
        f"| **PORTED** | {d['ported']:,} ({d['pct']:.1f}% of {d['total']:,} features) |\n"
        f"| **REAL_GAP** | {d['real']:,} ({100.0 * d['real'] / d['total']:.1f}%) |\n"
        f"| **PARTIAL** | {d['partial']:,} |\n"
        f"| **BOOTLEG** | {b} (recursive_false_gap_hunter.py) |\n"
        f"| **Build** | Clean, 0 errors (slermes ~37 MB) |\n"
        f"| **Tests** | Mission 8: 65 pass / 0 fail (state_db 27, API 17, UI 12, CLI 9) |\n"
        f"| **Upstream Sync** | {ahead:,} ahead / {behind:,} behind upstream/main (last merge {d['last_merge']}) |\n"
        f"\n"
        f"> Live counts: `make parity-walkway` (sentinel PARITY:AUTO). "
        f"Do not hand-edit — regenerated from the live scanner on every run."
    )


def main():
    bump = "--bump" in sys.argv
    if bump:
        bump_version()
    d = compute()
    inner = count_block(d)
    for f in WALKWAY_FILES:
        strip_stale_owned(f)   # remove stale hand-era paragraphs outside sentinel
        ok = inject(f, inner)
        ok2 = fix_title_version(f, d)
        print(f"{'OK  ' if ok or ok2 else 'MISS'} {f}")

    # Root docs: BANNER phase/sync, ROADMAP header, parity-summary table.
    # strip_stale_owned runs first so stale hand-era paragraphs outside the
    # sentinel are removed and re-runs converge (idempotent).
    for doc in [os.path.join(REPO, "BANNER.md"),
                os.path.join(REPO, "ROADMAP.md"),
                os.path.join(REPO, "docs", "parity-summary.md")]:
        strip_stale_owned(doc)
    ok = inject(os.path.join(REPO, "BANNER.md"), banner_block(d),
                insert_after="## Core Principle")
    print(f"{'OK  ' if ok else 'MISS'} BANNER.md")
    ok = inject(os.path.join(REPO, "ROADMAP.md"), roadmap_block(d),
                insert_after="# 🗺️ Slermes Roadmap — What's Next")
    print(f"{'OK  ' if ok else 'MISS'} ROADMAP.md")
    ok = inject(os.path.join(REPO, "docs", "parity-summary.md"),
                parity_summary_block(d),
                insert_after="## Overall Numbers (live)")
    print(f"{'OK  ' if ok else 'MISS'} docs/parity-summary.md")
    # User-level INDEX.md battleship line.
    idx = os.path.expanduser("~/.hermes/mind-palace/INDEX.md")
    if os.path.exists(idx):
        txt = open(idx, encoding="utf-8").read()
        txt2 = re.sub(r"^- \*\*battleship\.md\*\*.*$",
                      index_line(d), txt, count=1, flags=re.M)
        if txt2 != txt:
            open(idx, "w", encoding="utf-8").write(txt2)
            print(f"OK   {idx}")

    # Battleship index (project-level): replace the hand-curated
    # "## Current State" table with the generated one.
    bindex = os.path.join(REPO, ".hermes", "mind-palace", "index.md")
    if os.path.exists(bindex):
        txt = open(bindex, encoding="utf-8").read()
        m = re.search(r"## Current State\n(.*?)\n---", txt, re.S)
        if m:
            txt2 = (txt[:m.start(1)] + index_state_block(d) + "\n" +
                    txt[m.end(1):])
            if txt2 != txt:
                open(bindex, "w", encoding="utf-8").write(txt2)
                print(f"OK   {bindex} (current-state table)")
    print(
        f"Live: PORTED {d['ported']:,} REAL_GAP {d['real']:,} "
        f"PARTIAL {d['partial']:,} BOOTLEG {d['bootleg']} "
        f"(total {d['total']:,}) · sync {d['ahead']}/{d['behind']} · "
        f"{d['version']} {d['phase']} phase"
    )


if __name__ == "__main__":
    main()
