#!/usr/bin/env python3
"""gap_queue.py — the single source of truth for Slermes gap-closing work.

Regenerates a ranked, machine-readable work queue from LIVE reality data
(tests/parity_reality.py internals), never from hand-tallied counts:

  python scripts/gap_queue.py --regen          # rebuild docs/dev/gap-queue.jsonl
  python scripts/gap_queue.py --top 10         # next queued bites
  python scripts/gap_queue.py --claim ID --owner NAME
  python scripts/gap_queue.py --land ID --commit HASH --evidence LOGPATH

Entry kinds:
  ph:<c_file>    placeholder cluster — every PoP-claimed body in that C file
                 that parity_reality judges a placeholder. In-place re-port
                 rule applies (REORGANIZATION.md): replace the body, keep the
                 claim honest, caller + nm proof, leaf oracle, then land.
  dbg:<name>     open oracle-debug target seeded from docs/dev/debug-queue.md.
  dead:triage    connect-or-strip triage of the inherited dead wired objects
                 (tests/check_dead_ports.py baseline).

Status flow: queued -> claimed -> verified -> landed.  The generator never
invents progress: an entry whose placeholders vanished from reality is marked
'gone-from-reality' (needs oracle evidence before it counts), and landed
entries keep their commit hash + evidence path for audit.  Re-running --regen
preserves status/owner/evidence/commit of every entry whose id still exists.
"""
import argparse
import importlib.util
import json
import pathlib
import sys
from collections import defaultdict

REPO = pathlib.Path(__file__).resolve().parent.parent
QUEUE = REPO / "docs" / "dev" / "gap-queue.jsonl"


def load_reality():
    spec = importlib.util.spec_from_file_location(
        "parity_reality", REPO / "tests" / "parity_reality.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def reality_clusters():
    """c_file -> [ {py, fn, c} ] for every placeholder, plus module totals."""
    r = load_reality()
    feats = r.python_features()
    defs = r.c_definitions()
    pops = r.c_names_by_pop()
    by_name = {r.norm(n): n for n in defs}
    clusters = defaultdict(list)
    missing = defaultdict(int)
    for mod, fn in sorted(feats):
        cname = (pops.get((mod, fn)) or by_name.get(r.norm(fn))
                 or by_name.get(r.norm(mod.split("/")[-1][:-3] + "_" + fn)))
        if cname is None or cname not in defs:
            missing[mod.split("/")[0]] += 1
            continue
        cfile, _body, is_ph = defs[cname]
        if is_ph:
            clusters[cfile].append({"py": mod, "fn": fn, "c": cname})
    return clusters, dict(missing)


def read_queue():
    entries = {}
    if QUEUE.exists():
        for ln in QUEUE.read_text(encoding="utf-8").splitlines():
            ln = ln.strip()
            if not ln:
                continue
            e = json.loads(ln)
            entries[e["id"]] = e
    return entries


def write_queue(entries):
    QUEUE.parent.mkdir(parents=True, exist_ok=True)
    with QUEUE.open("w", encoding="utf-8", newline="\n") as f:
        for e in entries:
            f.write(json.dumps(e, ensure_ascii=False) + "\n")


def regen():
    old = read_queue()
    clusters, missing = reality_clusters()
    out = []
    for cfile, feats in sorted(clusters.items(),
                               key=lambda kv: (-len(kv[1]), kv[0])):
        eid = "ph:" + cfile
        prev = old.get(eid, {})
        e = {
            "id": eid, "kind": "placeholder-cluster", "c_file": cfile,
            "count": len(feats), "features": feats,
            "status": prev.get("status", "queued"),
            "owner": prev.get("owner"), "evidence": prev.get("evidence"),
            "commit": prev.get("commit"),
        }
        if prev.get("status") in ("claimed", "verified", "landed"):
            # reality no longer sees these placeholders -> body changed;
            # keep the claim but flag it for evidence re-check.
            e["status"] = "gone-from-reality"
            e["note"] = "placeholders no longer detected; re-verify oracle"
        out.append(e)
    # preserve non-placeholder entries (dbg:*, dead:*) untouched
    for eid, e in old.items():
        if not eid.startswith("ph:"):
            out.append(e)
    write_queue(out)
    tot = sum(e["count"] for e in out if e["kind"] == "placeholder-cluster")
    print(f"queue: {len(out)} entries, {tot} placeholders in "
          f"{sum(1 for e in out if e['kind'] == 'placeholder-cluster')} clusters")
    print("missing by area:", json.dumps(missing, sort_keys=True))
    return 0


def top(n):
    entries = [e for e in read_queue().values()
               if e.get("status") == "queued"]
    entries.sort(key=lambda e: (-e.get("count", 0), e["id"]))
    for e in entries[:n]:
        print(f"{e['id']}  count={e.get('count', '-')}  {e['kind']}")
    return 0


def update(eid, **fields):
    entries = read_queue()
    if eid not in entries:
        print(f"no such entry: {eid}", file=sys.stderr)
        return 1
    entries[eid].update({k: v for k, v in fields.items() if v is not None})
    write_queue(list(entries.values()))
    print(json.dumps(entries[eid], ensure_ascii=False)[:400])
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--regen", action="store_true")
    ap.add_argument("--top", type=int, default=0)
    ap.add_argument("--claim")
    ap.add_argument("--land")
    ap.add_argument("--verify")
    ap.add_argument("--owner")
    ap.add_argument("--commit")
    ap.add_argument("--evidence")
    args = ap.parse_args()
    if args.regen:
        return regen()
    if args.top:
        return top(args.top)
    if args.claim:
        return update(args.claim, status="claimed", owner=args.owner)
    if args.verify:
        return update(args.verify, status="verified", evidence=args.evidence)
    if args.land:
        return update(args.land, status="landed", commit=args.commit,
                      evidence=args.evidence)
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())