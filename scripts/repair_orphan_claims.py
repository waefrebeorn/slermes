#!/usr/bin/env python3
"""Repair orphan PoP claims: re-point module paths to where the claimed
functions actually live in the current quarry (function-name evidence).

Dry-run by default; --apply rewrites annotations in src/ and include/.
"""
import ast, collections, json, os, re, sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
QUARRY = Path("C:/Users/eman5/hermes-agent")
APPLY = "--apply" in sys.argv

# 1. Index live defs: fn_name -> set of module paths (skip slermes/ mirror dir)
fn_index = collections.defaultdict(set)
mod_files = []
for root, dirs, files in os.walk(QUARRY):
    dirs[:] = [d for d in dirs if d not in (".git", "node_modules", ".venv", "__pycache__", "build", "dist", "slermes", ".testbin")]
    for f in files:
        if not f.endswith(".py"):
            continue
        p = Path(root) / f
        rel = p.relative_to(QUARRY).as_posix()
        mod_files.append(rel)
        try:
            tree = ast.parse(p.read_text(encoding="utf-8", errors="replace"))
        except Exception:
            continue
        for node in ast.walk(tree):
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                fn_index[node.name].add(rel)
            elif isinstance(node, ast.ClassDef):
                for sub in node.body:
                    if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)):
                        fn_index[f"{node.name}.{sub.name}"].add(rel)
print(f"indexed {len(mod_files)} modules, {len(fn_index)} names", flush=True)

# 2. Collect PoP annotations from C sources
POP = re.compile(r"/\*\s*PoP:\s*([A-Za-z_0-9]+)\s*@\s*([A-Za-z_0-9./]+\.py):([A-Za-z_0-9.]+)\s*\*/")
live_set = set(mod_files)
claims = []  # (file, line_no, c_name, module, py_fn)
for base in ("src", "include"):
    for root, dirs, files in os.walk(REPO / base):
        dirs[:] = [d for d in dirs if d not in ("__pycache__",)]
        for f in files:
            if not f.endswith((".c", ".h")):
                continue
            p = Path(root) / f
            try:
                lines = p.read_text(encoding="utf-8", errors="replace").splitlines()
            except Exception:
                continue
            for i, line in enumerate(lines):
                m = POP.search(line)
                if m:
                    claims.append((p, i, m.group(1), m.group(2), m.group(3)))
print(f"claims: {len(claims)}", flush=True)

# 3. Group orphan claims by module
orphan_mods = collections.defaultdict(list)
for p, i, c, mod, fn in claims:
    if mod not in live_set:
        orphan_mods[mod].append((p, i, c, fn))
print(f"orphan modules: {len(orphan_mods)}, orphan claims: {sum(len(v) for v in orphan_mods.values())}", flush=True)

# 4. Resolve each orphan CLAIM individually by function evidence.
#    Test/eval mirrors are not real homes; the sqlite implementation is the
#    home for ports written against the old sqlite hermes_state code.
BAD_PREFIX = ("tests/", "evals/", "optional-skills/", "slermes/", "skills/", "scripts/")
GENERIC = {"main", "send", "run", "handle", "format", "parse", "init", "setup",
           "get", "set", "load", "save", "call", "process", "handle_message",
           "on_message", "start", "stop", "close", "connect", "dispatch",
           "execute", "build", "create", "update", "delete", "render", "emit",
           "validate", "normalize", "resolve", "apply", "check", "reply"}
def real_homes(cands):
    return sorted(c for c in cands if not c.startswith(BAD_PREFIX))

repairs = {}   # old_mod -> new_mod (module-level, for reporting)
claim_fixes = []  # (file, line, old_mod, new_mod)
unresolved = {}
for mod, entries in sorted(orphan_mods.items()):
    # dotted-path malformations: hermes_cli.completion.py -> hermes_cli/completion.py
    normalized = mod[:-3].replace(".", "/") + ".py"
    if normalized != mod and normalized in live_set:
        for p, i, c, fn in entries:
            claim_fixes.append((p, i, mod, normalized))
        repairs[mod] = normalized
        continue
    fixed = 0
    why = collections.Counter()
    for p, i, c, fn in entries:
        if fn.split(".")[-1] in GENERIC:
            why["generic-name-skipped"] += 1
            continue
        cands = fn_index.get(fn) or fn_index.get(fn.split(".")[-1])
        homes = real_homes(cands) if cands else []
        if len(homes) == 1:
            claim_fixes.append((p, i, mod, homes[0]))
            fixed += 1
        elif len(homes) > 1:
            # prefer the sqlite implementation, then same-top-level package
            sqlite = [h for h in homes if h.startswith("persistence/sqlite/")]
            pkg = mod.split("/")[0]
            same_pkg = [h for h in homes if h.split("/")[0] == pkg]
            pick = sqlite or same_pkg
            if len(pick) == 1:
                claim_fixes.append((p, i, mod, pick[0]))
                fixed += 1
            else:
                why["ambiguous:" + ",".join(homes[:3])] += 1
        else:
            why["deleted-or-renamed-fn"] += 1
    if fixed:
        targets = collections.Counter(n for _, _, o, n in claim_fixes if o == mod)
        repairs[mod] = targets.most_common(1)[0][0] + f" (+{fixed} claims)"
    # Consensus gate: a module whose claims mostly DON'T resolve is noise —
    # drop its fixes rather than re-point a minority on generic-name evidence.
    if len(entries) > 1 and fixed < 0.5 * len(entries):
        claim_fixes = [cf for cf in claim_fixes if cf[2] != mod]
        repairs.pop(mod, None)
        unresolved[mod] = {"low-consensus-dropped": len(entries) - fixed}
    elif fixed < len(entries):
        unresolved[mod] = dict(why.most_common(3))

print(f"\nREPAIRS ({len(repairs)}):")
for old, new in sorted(repairs.items()):
    print(f"  {old} -> {new}  ({len(orphan_mods[old])} claims)")
print(f"\nUNRESOLVED ({len(unresolved)}):")
for mod, why in sorted(unresolved.items()):
    print(f"  {mod}: {why}")

if APPLY:
    changed = collections.Counter()
    by_file = collections.defaultdict(list)
    for p, i, old, new in claim_fixes:
        by_file[p].append((i, old, new))
    for p, edits in by_file.items():
        lines = p.read_text(encoding="utf-8").splitlines(keepends=True)
        for i, old, new in edits:
            lines[i] = lines[i].replace(f"@ {old}:", f"@ {new}:")
            changed[old] += 1
        p.write_text("".join(lines), encoding="utf-8", newline="\n")
    print(f"\nAPPLIED: {sum(changed.values())} annotations in {len(by_file)} files")
json.dump({"repairs": repairs, "unresolved": {k: str(v) for k, v in unresolved.items()}},
          open(REPO / "scripts" / "orphan_repair_map.json", "w", encoding="utf-8"), indent=1)
print("map written: scripts/orphan_repair_map.json")
