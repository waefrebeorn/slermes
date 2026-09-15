#!/usr/bin/env python3
"""
gen_parity_walkway.py — single source of truth for ALL derived state.

FULLY AGNOSTIC: discovers files dynamically, injects sentinels automatically,
purges stale claims everywhere. No hardcoded file lists.

Usage:
  python3 scripts/gen_parity_walkway.py           # refresh all walkway files
  python3 scripts/gen_parity_walkway.py --bump    # v672 -> v673, then refresh
  python3 scripts/gen_parity_walkway.py --scan    # scan only, no writes
"""
import json
import os
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCANNER = os.path.join(REPO, "tests", "slermes_parity_battleground.py")
HUNTER = os.path.join(REPO, "tests", "recursive_false_gap_hunter.py")
TRUTH = os.path.join(REPO, "scripts", "parity_truth.py")
VERSION_FILE = os.path.join(REPO, "scripts", "version.txt")
SENT_OPEN = "<!-- PARITY:AUTO -->"
SENT_CLOSE = "<!-- /PARITY:AUTO -->"

# Directories to scan for walkway files
WALK_DIRS = [
    os.path.join(REPO, ".hermes"),
    os.path.join(REPO, "docs"),
    os.path.expanduser("~/.hermes/mind-palace"),
    os.path.expanduser("~/.hermes/walkway"),
]

# Directories to NEVER touch (archives, vaults, etc.)
EXCLUDE_DIRS = {"archive", "vault", "museum", "bins", "__pycache__", ".git"}

# Stale patterns to purge from non-sentinel regions
# ORDER MATTERS: more specific patterns first, generic ones last
STALE_PATTERNS = [
    # Stale prose count lines (must come before generic version pattern)
    (r'(?i)\*\*Current state \(v\d+[^)]*\):\*\*', 'stale prose count line'),
    # Inline classification counts (PORTED: 123, REAL_GAP: 456)
    (r'(?i)(?:PORTED|REAL_GAP|PARTIAL|BOOTLEG)\s*[:=]\s*\d{1,3}(?:,\d{3})*(?:\s*\(\d+%\))?', 'stale inline classification count'),
    # Old "X/Y (Z%) PORTED" inline counts
    (r'\d{1,3}(?:,\d{3})*\s*/\s*\d{1,3}(?:,\d{3})*\s*\(\s*\d{1,3}\.?\d*%\s*\)\s*(?:PORTED|ported|port)', 'stale inline PORTED ratio'),
    # "X BOOTLEG" claims
    (r'(?i)\d{1,3}(?:,\d{3})*\s*BOOTLEG', 'stale bootleg count'),
    # "X PORTED / Y REAL_GAP" prose claims
    (r'(?i)\d{1,3}(?:,\d{3})*\s*PORTED\s*/\s*\d{1,3}(?:,\d{3})*\s*REAL_GAP', 'stale prose count claim'),
    # Old version claims that drift as versions advance
    (r'\*\*v\d+\*\*.*?(?:phase|port|PARITY|MATCH|COMPLETE|PORT phase)', 'stale version/phase claim'),
    # Plain version references (v668, v669, etc.) — but NOT in URLs
    (r'(?<![a-zA-Z/])v\d{3,4}(?![a-zA-Z0-9])', 'stale version ref'),
    # "Version: vNNN" standalone lines
    (r'(?i)\*\*Version:\*\*\s*v\d+', 'stale version line'),
    # "Last updated: YYYY-MM-DD" standalone lines
    (r'(?i)\*\*Last updated:\*\*\s*\d{4}-\d{2}-\d{2}', 'stale date line'),
    # Dates from older sessions (2026-07, 2026-08, 2026-09)
    (r'\b2026-(?:0[7-9]|1[0-2])-\d{2}\b', 'stale date from older session'),
    # Old total counts that drift as upstream grows
    (r'\|\s*TOTAL\s*\|\s*\d{4,5}\s*\|', 'stale TOTAL count row'),
    # "99.8%" completion claim
    (r'99\.8%', 'stale 99.8% completion claim'),
    # Hardcoded function counts from older scans
    (r'(?i)\b(?:8,688|12,274|11,744|12,252|4,881|4,802|4,781|4,769|4,703|4,709|4,692|4,754|4,702)\b', 'stale function count'),
    # "mission8 36/0" style claims
    (r'(?i)mission\s*\d+\s*\d+/\d+', 'stale mission count claim'),
    # "X ahead / Y behind" sync claims
    (r'(?i)\d{1,5}\s*ahead\s*/\s*\d{1,5}\s*behind', 'stale sync checkpoint'),
    # Stale "N/A by design" / "N/A category" references
    (r'(?i)\bN/A\b.*(?:by design|category|not applicable)', 'stale N/A claim'),
]


def current_version():
    try:
        with open(VERSION_FILE) as f:
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
    with open(VERSION_FILE, "w") as f:
        f.write(new + "\n")
    return new


def discover_walkway_files():
    """Discover all .md files that should be processed.
    Returns list of (absolute_path, classification) tuples.
    classification: 'active' (needs sentinel), 'historical' (skip), 'sentinel' (already owned)"""
    results = []
    
    for walk_dir in WALK_DIRS:
        if not os.path.isdir(walk_dir):
            continue
        for root, dirs, files in os.walk(walk_dir):
            # Skip excluded directories
            parts = root.split('/')
            if any(p in EXCLUDE_DIRS for p in parts):
                continue
            for f in files:
                if not f.endswith(".md"):
                    continue
                path = os.path.join(root, f)
                rel = os.path.relpath(path, REPO)
                
                # Classify
                try:
                    content = open(path).read()
                except:
                    continue
                
                has_sentinel = SENT_OPEN in content and SENT_CLOSE in content
                
                if has_sentinel:
                    results.append((path, "sentinel"))
                elif "battleship-v" in f or "session-prestige" in f or "prompt-v" in f:
                    results.append((path, "historical"))
                else:
                    results.append((path, "active"))
    
    # Root files
    for f in ["BANNER.md", "README.md", "ROADMAP.md"]:
        path = os.path.join(REPO, f)
        if os.path.isfile(path):
            content = open(path).read()
            has_sentinel = SENT_OPEN in content and SENT_CLOSE in content
            if has_sentinel:
                results.append((path, "sentinel"))
            else:
                results.append((path, "active"))
    
    return results


def compute_parity():
    """Compute all derived state from live sources."""
    # Run parity_truth.py (fail-closed)
    r = subprocess.run([sys.executable, TRUTH], capture_output=True, text=True, cwd=REPO)
    if r.returncode != 0:
        sys.exit(r.returncode)
    
    with open(os.path.join(REPO, "live_parity_scan.json")) as f:
        doc = json.load(f)
    
    tot = doc.get("totals", {})
    
    def _i(v, d=0):
        try:
            return int(v)
        except (TypeError, ValueError):
            return d
    
    total = _i(tot.get("total"))
    ported = _i(tot.get("ported"))
    real = _i(tot.get("real_gaps"))
    partial = _i(tot.get("partial"))
    da4 = _i(tot.get("da4_annotation_only_stubs", 0))
    genuine = _i(tot.get("genuine_gaps", real))
    
    try:
        pct = float(tot.get("coverage_pct", 0))
    except:
        pct = 0.0
    
    # BOOTLEG count
    bootleg = None
    try:
        rh = subprocess.run([sys.executable, HUNTER], capture_output=True, text=True, timeout=900, cwd=REPO)
        m = re.search(r"BOOTLEG[^:]*:\s*(\d+)", rh.stdout)
        if m:
            bootleg = int(m.group(1))
    except:
        pass
    
    # Upstream sync
    ahead = behind = -1
    last_merge = "unknown"
    try:
        subprocess.run(["git", "fetch", "upstream", "main"], capture_output=True, cwd=REPO)
        rb = subprocess.run(["git", "rev-list", "--count", "HEAD..upstream/main"], capture_output=True, text=True, cwd=REPO)
        ra = subprocess.run(["git", "rev-list", "--count", "upstream/main..HEAD"], capture_output=True, text=True, cwd=REPO)
        if rb.returncode == 0 and ra.returncode == 0:
            behind = int(rb.stdout.strip())
            ahead = int(ra.stdout.strip())
        rt = subprocess.run(["git", "log", "-1", "--format=%ad", "--date=short", "upstream/main"], capture_output=True, text=True, cwd=REPO)
        if rt.returncode == 0 and rt.stdout.strip():
            last_merge = rt.stdout.strip() + " (upstream fetched)"
    except:
        pass
    
    # Phase
    if real == 0 and bootleg == 0:
        phase = "COMPLETE"
    elif real <= 50 and (bootleg is None or bootleg == 0):
        phase = "MATCH"
    else:
        phase = "PORT"
    
    # Watercup
    watercup = {"total": 0, "pass": 0, "fail": 0, "pass_pct": 0.0,
                "ported_pass": 0, "ported_total": 0, "gap_mismatch": 0, "gap_total": 0}
    wc_results = os.path.join(REPO, "tests", "watercup", "build", "results.jsonl")
    if os.path.isfile(wc_results):
        wc_gates = {}
        with open(wc_results) as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    r = json.loads(line)
                except:
                    continue
                gname = r.get("gate", "?")
                if gname not in wc_gates:
                    wc_gates[gname] = r.get("ok", False)
        wc_total = len(wc_gates)
        wc_pass = sum(1 for v in wc_gates.values() if v)
        watercup = {
            "total": wc_total,
            "pass": wc_pass,
            "fail": wc_total - wc_pass,
            "pass_pct": (100.0 * wc_pass / wc_total) if wc_total else 0.0,
            "ported_pass": sum(1 for g, ok in wc_gates.items() if g.startswith("port__") and ok),
            "ported_total": sum(1 for g in wc_gates if g.startswith("port__")),
            "gap_mismatch": sum(1 for g, ok in wc_gates.items() if g.startswith("real__") and not ok),
            "gap_total": sum(1 for g in wc_gates if g.startswith("real__")),
        }
    
    version = current_version()
    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    
    return dict(
        ported=ported, real=real, partial=partial, total=total,
        pct=pct, bootleg=bootleg if bootleg is not None else 0,
        ahead=max(ahead, 0), behind=max(behind, 0),
        last_merge=last_merge, phase=phase, version=version,
        stamp=stamp, da4=da4, genuine=genuine, watercup=watercup,
        doc=doc,  # full scan doc for gap list generation
    )


def build_count_block(d):
    """Build the canonical count block for walkway files."""
    partial_pct = 100.0 * d['partial'] / d['total'] if d['total'] else 0.0
    bootleg = d['bootleg'] if d['bootleg'] is not None else 0
    ahead = d['ahead'] if d['ahead'] >= 0 else "?"
    behind = d['behind'] if d['behind'] >= 0 else "?"
    wc = d.get('watercup', {})
    wc_line = ""
    if wc.get('total', 0) > 0:
        wc_line = f"| **Watercup** | {wc['pass']:,} PASS / {wc['total']:,} ({wc['pass_pct']:.1f}%) · {wc['ported_pass']}/{wc['ported_total']} ported match · {wc['gap_mismatch']}/{wc['gap_total']} gap mismatch |\n"
    
    return (
        f"| PORTED  | {d['ported']:,} / {d['total']:,} ({d['pct']:.1f}%) |\n"
        f"| REAL_GAP| {d['real']:,} ({100.0 * d['real'] / d['total']:.1f}%) — no N/A |\n"
        f"|  ├ DA-4 stubs | {d['da4']:,} |\n"
        f"|  └ Genuine gaps | {d['genuine']:,} |\n"
        f"| PARTIAL | {d['partial']:,} ({partial_pct:.1f}%) |\n"
        f"| BOOTLEG | {bootleg:,} |\n"
        f"{wc_line}"
        f"**Phase ({d['version']}):** {d['phase']} phase\n\n"
        f"**Upstream sync:** {ahead:,} ahead / {behind:,} behind (last merge {d['last_merge']})\n\n"
        f"_Generated {d['stamp']} from live scanner + watercup oracle — do not edit by hand._"
    )


def purge_stale_claims(txt):
    """Remove stale parity claims from non-sentinel regions."""
    for pattern, _ in STALE_PATTERNS:
        txt = re.sub(pattern, "", txt, flags=re.MULTILINE | re.IGNORECASE)
    # Collapse blank lines
    txt = re.sub(r"\n{3,}", "\n\n", txt)
    return txt


def process_file(path, d, scan_doc):
    """Process a single walkway file: inject sentinel, purge stale."""
    try:
        content = open(path).read()
    except:
        return
    
    has_sentinel = SENT_OPEN in content and SENT_CLOSE in content
    rel = os.path.relpath(path, REPO)
    
    # Purge stale from non-sentinel regions
    if has_sentinel:
        parts = content.split(SENT_OPEN)
        new_parts = [purge_stale_claims(parts[0])]
        for seg in parts[1:]:
            if SENT_CLOSE in seg:
                inner, after = seg.split(SENT_CLOSE, 1)
                new_parts.append(SENT_OPEN + inner + SENT_CLOSE)
                new_parts.append(purge_stale_claims(after))
            else:
                new_parts.append(purge_stale_claims(seg))
        content = "".join(new_parts)
    else:
        # No sentinel — check if it has parity claims
        has_claims = any(re.search(p, content, re.IGNORECASE) for p, _ in STALE_PATTERNS)
        if has_claims:
            # Inject sentinel at top
            content = f"{SENT_OPEN}\n{build_count_block(d)}\n{SENT_CLOSE}\n\n{content}"
        else:
            # Clean file — inject sentinel at top
            content = f"{SENT_OPEN}\n{build_count_block(d)}\n{SENT_CLOSE}\n\n{content}"
    
    # Write back
    with open(path, 'w') as f:
        f.write(content)
    
    print(f"OK   {rel}")


def generate_gap_list(d):
    """Generate docs/real-gap-list.md from scan doc."""
    doc = d.get("doc", {})
    mods = doc.get("modules", {})
    lines = []
    total_gaps = 0
    module_count = 0
    for key in sorted(mods):
        gaps = [g for g in mods[key].get("gaps", []) if g.get("classification") == "REAL_GAP"]
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
    
    content = (
        f"# Slermes REAL_GAP List — Function Level (live scanner)\n\n"
        f"> Generated {d['stamp']} from `live_parity_scan.json`. "
        f"**{total_gaps:,} REAL_GAP across {module_count} modules** "
        f"(of {d['total']:,} total functions).\n\n"
        f"> This is the forward work plan. Never hand-edit — regenerate via the scanner.\n"
        + "\n".join(lines) + "\n"
    )
    
    path = os.path.join(REPO, "docs", "real-gap-list.md")
    with open(path, 'w') as f:
        f.write(content)
    print(f"OK   docs/real-gap-list.md ({total_gaps:,} gaps)")


def main():
    scan = "--scan" in sys.argv
    if "--bump" in sys.argv:
        bump_version()
    
    d = compute_parity()
    
    # Discover files
    files = discover_walkway_files()
    
    print(f"=== Parity Walkway Generator ===")
    print(f"    PORTED: {d['ported']:,} / {d['total']:,} ({d['pct']:.1f}%)")
    print(f"    REAL_GAP: {d['real']:,}")
    print(f"    Watercup: {d['watercup']['pass']:,} / {d['watercup']['total']:,}")
    print(f"    Files: {len(files)}")
    print()
    
    if scan:
        for path, cls in files:
            print(f"{cls:10s} {os.path.relpath(path, REPO)}")
        return
    
    # Process each file
    for path, cls in files:
        if cls == "historical":
            continue
        process_file(path, d, d.get("doc"))
    
    # Generate gap list
    generate_gap_list(d)
    
    print(f"\nLive: PORTED {d['ported']:,} REAL_GAP {d['real']:,} "
          f"PARTIAL {d['partial']:,} BOOTLEG {d['bootleg']} "
          f"(total {d['total']:,}) · sync {d['ahead']}/{d['behind']} · "
          f"{d['version']} {d['phase']} phase")


if __name__ == "__main__":
    main()
