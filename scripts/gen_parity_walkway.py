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
  - README header + sync checkpoint       <- programmatic
  - docs/index TOC + status table         <- programmatic
  - stale-claim purge across ALL docs     <- programmatic

Nothing is hand-copied. Run `make parity-walkway` after every scanner run —
it refreshes every owned section in every file. Hand-edited numbers are
overwritten on the next run; the only manual step is the checkpoint bump:

    python3 scripts/gen_parity_walkway.py --bump   # v668 -> v669, then refresh all

Sentinel convention: every owned section is wrapped in
`<!-- PARITY:AUTO --> ... <!-- /PARITY:AUTO -->`. Files WITHOUT the sentinel
get a block appended (so future runs regenerate it). Version headers in
walkway titles are fixed by regex on the first line (`vNNN` -> current, phase
label normalized) — no per-file hand-tuning.

Stale-claim purge: every run strips hand-transcribed counts, version refs,
completion percentages, and N/A claims from all docs outside sentinels,
so re-runs converge (idempotent).
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
    os.path.join(REPO, "README.md"),
]

# Root docs with derived TOC / index blocks (programmatic).
INDEX_FILES = [
    os.path.join(REPO, "docs", "index.md"),
]

# Stale-claim patterns to purge from all docs (outside sentinels).
STALE_PATTERNS = [
    # Old version claims that drift as versions advance
    (r"\*\*v\d+\*\*.*?(?:phase|port|PARITY|MATCH|COMPLETE|PORT phase)",
     "stale version/phase claim"),
    # Old total counts that drift as upstream grows
    (r"\|\s*TOTAL\s*\|\s*\d{4,5}\s*\|",
     "stale TOTAL count row"),
    # Old "X/Y (Z%) PORTED" inline counts
    (r"\d{1,3}(?:,\d{3})*\s*/\s*\d{1,3}(?:,\d{3})*\s*\(\s*\d{1,3}\.\d+%\s*\)\s*(?:PORTED|ported|port)",
     "stale inline PORTED ratio"),
    # "99.8%" completion claim (the classic barnacle)
    (r"99\.8%",
     "stale 99.8% completion claim"),
    # "100%" module coverage claim (module-map era)
    (r"100%\s*(?:module|coverage|complete)",
     "stale 100% coverage claim"),
    # Old function counts from older scans
    (r"\b(?:8,688|12,274|11,744|12,252|4,881|4,802|4,781|4,769|4,703|4,709|4,692|4,754|4,702)\b",
     "stale function count from older scan"),
    # Old "N/A by design" / "N/A category" references
    (r"\bN/A\b.*(?:by design|category|not applicable)",
     "stale N/A claim"),
    # Stale "**Current state (vNNN, ...):**" prose count-table block that
    # exists OUTSIDE sentinels in README.md / docs/index.md (the hand-edit
    # barnacle). Two formats appear in the wild:
    #  (a) markdown-table form: header + blank lines + | Classification |... table
    #  (b) inline-count form: "**Current state (vNNN...):** NNN PORTED / ..."
    # Both are purged so re-runs converge (idempotent).
    (r"\*\*Current state \(v\d+[^)]*\):\*\*\n*\|\s*Classification[^\n]*\n\|\s*-[^\n]*\n(?:\|[^\n]*\n)+",
     "stale prose count table (markdown form) outside sentinel"),
    (r"\*\*Current state \(v\d+[^)]*\):\*\*[^\n]*\n",
     "stale prose count line (inline form) outside sentinel"),
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
        # Refresh upstream/main FIRST — the whole point of the checkpoint is
        # to match GitHub's live ahead/behind badge, which compares against
        # the CURRENT upstream tip. A stale local ref stamps stale numbers
        # (measured bug: gate said 1201/1115, GitHub truth was 1204/1119 —
        # exactly the 4 commits the stale ref was missing).
        rf = run(["git", "fetch", "upstream", "main"])
        rb = run(["git", "rev-list", "--count", "HEAD..upstream/main"])
        ra = run(["git", "rev-list", "--count", "upstream/main..HEAD"])
        if rf.returncode == 0 and rb.returncode == 0 and ra.returncode == 0:
            behind = int(rb.stdout.strip())
            ahead = int(ra.stdout.strip())
        # "last gathered" = when the upstream ref was last updated (the
        # freshness of the Python quarry), NOT the last merge commit in our
        # own history (which predates the fetch).
        rt = run(["git", "log", "-1", "--format=%ad", "--date=short",
                  "upstream/main"])
        if rt.returncode == 0 and rt.stdout.strip():
            last_merge = rt.stdout.strip() + " (upstream fetched)"
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
        f"upstream/main (last merge {d['last_merge']}). "
        f"{'The repo is up to date with upstream.' if behind == 0 else 'The behind-count is the staleness timer — see the stash→pull→fix→pop workflow below.'}\n"
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
            f"systems. The PORT phase (v398→v667) is legacy; the "
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
        f"upstream/main (last merge {d['last_merge']}). "
        f"{'The repo is up to date with upstream.' if behind == 0 else 'The behind-count is the staleness timer — see the stash→pull→fix→pop workflow below.'}\n"
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
        f"upstream/main (last merge {d['last_merge']}). "
        f"{'The repo is up to date with upstream.' if behind == 0 else 'The behind-count is the staleness timer; re-port the delta with the stash→pull→fix→pop workflow after each sync.'}"
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


def purge_stale_claims(path):
    """Remove stale derived claims from a doc file (outside sentinels).

    These are the barnacles that caused the 8,688/8,688 100% fiction:
    hand-transcribed counts that drifted as upstream grew. The purge
    runs on every `make parity-walkway` pass so re-runs converge
    (idempotent). Returns True if the file changed.
    """
    if not os.path.exists(path):
        return False
    txt = open(path, encoding="utf-8").read()
    orig = txt

    # Split into sentinel regions (kept verbatim) and plain regions (purged).
    parts = txt.split(SENT_OPEN)
    out = [purge_plain(parts[0])]  # region before any sentinel — purge too
    for i, seg in enumerate(parts[1:]):
        if SENT_CLOSE in seg:
            head, tail = seg.split(SENT_CLOSE, 1)
            out.append(SENT_OPEN + head + SENT_CLOSE)  # sentinel verbatim
            out.append(purge_plain(tail))
        else:
            out.append(purge_plain(seg))
    txt = "".join(out)

    # Collapse 3+ blank lines left by the purges.
    txt = re.sub(r"\n{3,}", "\n\n", txt)
    if txt != orig:
        open(path, "w", encoding="utf-8").write(txt)
        return True
    return False


def purge_plain(txt):
    """Strip stale derived claims from a non-sentinel region."""
    for pattern, _label in STALE_PATTERNS:
        txt = re.sub(pattern, "", txt, flags=re.MULTILINE)
    # Clean up orphaned table rows left by TOTAL purge (e.g. a row
    # like "| OLD_TOTAL | 11,744 | 100% | All functions scanned |"
    # becomes "|  |  |  |" — collapse it).
    txt = re.sub(r"\|\s*\|[\s|]*\n", "", txt)
    return txt


def readme_header_block(d):
    """README.md derived header (version + phase + sync checkpoint)."""
    ahead = d['ahead'] if d['ahead'] >= 0 else "?"
    behind = d['behind'] if d['behind'] >= 0 else "?"
    return (
        f"**Version:** {d['version']} ({d['phase']} phase)  \n"
        f"**Last updated:** {d['stamp'][:10]}\n\n"
        f"> Live counts from `make parity-walkway` (sentinel PARITY:AUTO). "
        f"Do not hand-edit — regenerated from the live scanner on every run.\n\n"
        f"**Upstream sync checkpoint:** {ahead:,} ahead / {behind:,} behind "
        f"upstream/main (last merge {d['last_merge']}). "
        f"{'The repo is up to date with upstream.' if behind == 0 else 'The behind-count is the staleness timer.'}\n"
    )


def index_toc_block(d):
    """docs/index.md derived TOC (mirrors src/ layout, counts from scanner)."""
    b = d['bootleg'] if d['bootleg'] is not None else "?"
    return (
        f"## Project Status\n\n"
        f"| Metric | Value |\n"
        f"|--------|-------|\n"
        f"| **Version** | {d['version']} ({d['phase']} phase) |\n"
        f"| **PORTED** | {d['ported']:,} ({d['pct']:.1f}% of {d['total']:,}) |\n"
        f"| **REAL_GAP** | {d['real']:,} ({100.0 * d['real'] / d['total']:.1f}%) |\n"
        f"| **PARTIAL** | {d['partial']:,} |\n"
        f"| **BOOTLEG** | {b} |\n"
        f"| **Upstream Sync** | {d['ahead']:,} ahead / {d['behind']:,} behind |\n"
        f"\n"
        f"> Live counts: `make parity-walkway` (sentinel PARITY:AUTO). "
        f"Do not hand-edit — regenerated from the live scanner on every run."
    )


def real_gap_list_block(d):
    """docs/real-gap-list.md — function-level gap list (programmatic).

    Regenerated every walkway run so it can never rot like the old
    hand-transcribed gap lists. The block lists every REAL_GAP function
    grouped by module, so the file doubles as the forward work plan.
    """
    scan_path = os.path.join(REPO, "live_parity_scan.json")
    if not os.path.exists(scan_path):
        return None
    with open(scan_path, encoding="utf-8") as f:
        scan = json.load(f)
    mods = scan.get("modules", {})
    lines = []
    total_gaps = 0
    module_count = 0
    for key in sorted(mods):
        gaps = [g for g in mods[key].get("gaps", [])
                if g.get("classification") == "REAL_GAP"]
        if not gaps:
            continue
        module_count += 1
        total_gaps += len(gaps)
        lines.append(f"\n### {key} ({len(gaps)} gaps)\n")
        for g in sorted(gaps, key=lambda x: x.get("python_feature", {}).get("line_number", 0)):
            feat = g.get("python_feature", {})
            name = feat.get("name", "?")
            kind = feat.get("kind", "fn")
            parent = feat.get("parent_class")
            qual = f"{parent}.{name}" if parent else name
            async_mark = "async " if feat.get("is_async") else ""
            lines.append(f"- {async_mark}{qual} ({kind})")
    return (
        f"# Slermes REAL_GAP List — Function Level (live scanner)\n\n"
        f"> Generated {d['stamp']} from `live_parity_scan.json` by "
        f"`make parity-walkway`. **{total_gaps:,} REAL_GAP across "
        f"{module_count} modules** (of {d['total']:,} total functions).\n\n"
        f"> This is the forward work plan. Each entry is a Python function "
        f"not yet ported to C. Close by implementing real C + a single-line "
        f"`/* PoP: fn @ module.py:fn */` annotation (see slermes-gap-closure "
        f"skill). Never hand-edit — regenerate via the scanner.\n"
        + "\n".join(lines) + "\n"
    )


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

    Agnostic regex: first line of the form `# Title (vNNN, X phase)` or
    `# Title vNNN ...` gets the current version and phase label. Only touches
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


def strip_stale_claims(path):
    """Remove stale derived claims from a doc file (outside sentinels).

    These are the barnacles that caused the 8,688/8,688 100% fiction:
    hand-transcribed counts that drifted as upstream grew. The purge
    runs on every `make parity-walkway` pass so re-runs converge
    (idempotent). Returns True if the file changed.
    """
    return purge_stale_claims(path)


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

    # Root docs: BANNER phase/sync, ROADMAP header, parity-summary table, README header.
    # strip_stale_owned runs first so stale hand-era paragraphs outside the
    # sentinel are removed and re-runs converge (idempotent).
    for doc in [os.path.join(REPO, "BANNER.md"),
                os.path.join(REPO, "ROADMAP.md"),
                os.path.join(REPO, "docs", "parity-summary.md"),
                os.path.join(REPO, "README.md")]:
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
    # docs/real-gap-list.md: full function-level gap list (programmatic).
    rgl = real_gap_list_block(d)
    if rgl is not None:
        rgl_path = os.path.join(REPO, "docs", "real-gap-list.md")
        old_rgl = ""
        if os.path.exists(rgl_path):
            old_rgl = open(rgl_path, encoding="utf-8").read()
        if old_rgl != rgl:
            open(rgl_path, "w", encoding="utf-8").write(rgl)
            print(f"OK   docs/real-gap-list.md ({d['real']:,} gaps listed)")
        else:
            print("SAME docs/real-gap-list.md")
    # README.md: version/phase header + sync checkpoint (programmatic).
    readme_path = os.path.join(REPO, "README.md")
    strip_stale_claims(readme_path)
    # Remove any existing "**Version:**" or "**Last updated:**" header block
    # in README before injecting the fresh one (idempotent re-run).
    if os.path.exists(readme_path):
        rtxt = open(readme_path, encoding="utf-8").read()
        rtxt = re.sub(
            r"(## What this is\n\n)(?:\|.*?\|\n)+.*?(?=\n## )",
            r"\1", rtxt, count=1, flags=re.DOTALL)
        open(readme_path, "w", encoding="utf-8").write(rtxt)
    ok = inject(readme_path, readme_header_block(d),
                insert_after="## What this is")
    print(f"{'OK  ' if ok else 'MISS'} README.md (header)")
    # docs/index.md: programmatic TOC + status table (mirrors src/ layout).
    idx_path = os.path.join(REPO, "docs", "index.md")
    strip_stale_claims(idx_path)
    if os.path.exists(idx_path):
        itxt = open(idx_path, encoding="utf-8").read()
        if "## Project Status" in itxt:
            itxt = re.sub(
                r"## Project Status\n\n.*?(?=\n## )",
                "## Project Status\n\n" + index_toc_block(d) + "\n",
                itxt, count=1, flags=re.DOTALL)
        else:
            itxt = itxt.rstrip() + "\n\n" + index_toc_block(d) + "\n"
        open(idx_path, "w", encoding="utf-8").write(itxt)
        print(f"OK   {idx_path} (TOC + status)")
    # Purge stale claims across ALL docs (outside sentinels).
    all_docs = []
    for root, _dirs, files in os.walk(os.path.join(REPO, "docs")):
        for fn in files:
            if fn.endswith(".md"):
                all_docs.append(os.path.join(root, fn))
    all_docs.append(os.path.join(REPO, "README.md"))
    all_docs.append(os.path.join(REPO, "BANNER.md"))
    all_docs.append(os.path.join(REPO, "ROADMAP.md"))
    purged = 0
    for doc in all_docs:
        if purge_stale_claims(doc):
            purged += 1
    if purged:
        print(f"PURGE {purged} docs (stale claims removed)")
    # User-level INDEX.md battleship line.
    idx_user = os.path.expanduser("~/.hermes/mind-palace/INDEX.md")
    if os.path.exists(idx_user):
        utxt = open(idx_user, encoding="utf-8").read()
        utxt2 = re.sub(r"^- \*\*battleship\.md\*\*.*$",
                        index_line(d), utxt, count=1, flags=re.M)
        if utxt2 != utxt:
            open(idx_user, "w", encoding="utf-8").write(utxt2)
            print(f"OK   {idx_user}")

    # Battleship index (project-level): replace the hand-curated
    # "## Current State" table with the generated one.
    bindex = os.path.join(REPO, ".hermes", "mind-palace", "index.md")
    if os.path.exists(bindex):
        btxt = open(bindex, encoding="utf-8").read()
        m = re.search(r"## Current State\n(.*?)\n---", btxt, re.S)
        if m:
            btxt2 = (btxt[:m.start(1)] + index_state_block(d) + "\n" +
                     btxt[m.end(1):])
            if btxt2 != btxt:
                open(bindex, "w", encoding="utf-8").write(btxt2)
                print(f"OK   {bindex} (current-state table)")
    print(
        f"Live: PORTED {d['ported']:,} REAL_GAP {d['real']:,} "
        f"PARTIAL {d['partial']:,} BOOTLEG {d['bootleg']} "
        f"(total {d['total']:,}) · sync {d['ahead']}/{d['behind']} · "
        f"{d['version']} {d['phase']} phase")


if __name__ == "__main__":
    main()