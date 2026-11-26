#!/usr/bin/env python3
"""Objects that do not define what their source defines.

The link is the last thing anyone checks, and it only fails on *missing* symbols. An object that
defines fewer symbols than its source does links fine as long as a stub elsewhere covers the gap --
that is how src/acp/server.o ended up with one global symbol while src/acp/server.c defines three,
and how `kanban_create` answered from strdup("task_id").

    python scripts/verify_objects.py              # report every disagreeing object
    python scripts/verify_objects.py --fix        # touch those sources so make rebuilds them
    python scripts/verify_objects.py --json out.json

Only non-static definitions at file scope are considered, and a symbol counts as defined when some
object in the tree exports it globally -- so a function that legitimately lives in another translation
unit is not reported.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
NM = Path(r"C:\winlibs\mingw64\bin\nm.exe")

DEF_RE = re.compile(
    r"^(?!\s)(?:[A-Za-z_][\w \t\*]*?[\s\*])(\w+)\s*\(([^;{]*)\)\s*\{", re.M)


_COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
_STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"')


def strip_comments(text: str) -> str:
    """Definitions inside /* ... */ and // ... are not definitions.

    agent_pet atlas, delegate_tool and turn_retry_state each carry a commented-out definition beside
    the real one; without this the report claimed 31 missing symbols that were never compiled."""
    # Blank the comment bodies (keeping newlines so line-based regexes still work), then the string
    # literals, so a definition-like fragment inside either cannot match.
    text = _COMMENT_RE.sub(lambda m: "\n" * m.group(0).count("\n"), text)
    return _STRING_RE.sub('""', text)


def source_defs(path: Path) -> list[str]:
    text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
    out = []
    # A `static` prototype earlier in the file makes the later unstyled definition internal. That is
    # C, not a defect: memory_storage.c declares 32 vtable helpers static at the top and defines them
    # without the keyword lower down.
    static_names = set(re.findall(r"^\s*static\b[^;{]*?\b(\w+)\s*\(", text, re.M))
    for m in DEF_RE.finditer(text):
        name = m.group(1)
        if name in {"if", "for", "while", "switch", "return", "sizeof", "else"}:
            continue
        decl = text[m.start():m.end()]
        if re.match(r"^static\b", decl) or re.search(r"\bstatic\b", decl.split("(")[0]):
            continue
        if name in static_names:
            continue
        out.append(name)
    return sorted(set(out))



def object_defs_map(objs: list[Path]) -> dict[str, set[str]]:
    """Every symbol each object DEFINES, global or local -- batched.

    Name presence in the union of all objects is not ownership: a stub elsewhere can satisfy the name
    for the file that owns the implementation (that is how tools_init_all and i18n_t stayed hidden).
    And scripts/localize_dupes.py deliberately makes the losing copy of a duplicate local, so a
    correct object may hold its symbol as `t` rather than `T`. Requiring the definition in the
    object's own symbol table satisfies both.

    One nm per object costs 2m54s over this tree; batched (Windows caps the command line, hence the
    groups) it is a few seconds. nm prefixes each line with the file path when given several inputs.
    """
    out: dict[str, set[str]] = {str(o): set() for o in objs}
    for i in range(0, len(objs), 300):
        group = objs[i:i + 300]
        try:
            r = subprocess.run([NM] + [str(o) for o in group], capture_output=True, text=True,
                               encoding="utf-8", errors="replace")
        except OSError:
            continue
        # nm with several inputs prints a "<file>:" header and then bare symbol lines; the earlier
        # version assumed "path: symbol" on every line and reported 4,309 objects as missing
        # hundreds of symbols each.
        cur = None
        for line in r.stdout.splitlines():
            st = line.strip()
            if not st:
                continue
            if st.endswith(":") and " " not in st:
                cur = st[:-1].strip()
                continue
            parts = st.split()
            if cur and len(parts) >= 2 and parts[-2] != "U":
                if cur in out:
                    out[cur].add(parts[-1])
    return out


def platform_excluded(src: Path, name: str) -> bool:
    """True when the definition of `name` sits in a branch this build compiles out.

    Walks back from the definition to the nearest #if/#ifdef/#elif whose condition names a
    platform, and answers yes when that condition cannot hold on the host we build for.
    """
    try:
        lines = src.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return False
    idx = None
    pat = re.compile(r"^\s*(?:[A-Za-z_][\w \t\*]*\s+)?\*?\s*" + re.escape(name) + r"\s*\(")
    for i, line in enumerate(lines):
        if not pat.match(line) or line.lstrip().startswith(("#", "//", "*")):
            continue
        # A prototype and the definition both match; only the definition opens a body. Without
        # this check run_command_pty's forward declaration was picked and its `#if defined(__linux__)`
        # guard -- which sits directly above the definition -- was never seen.
        tail = line[line.find("(") :]
        if "{" in tail or any(l.strip().startswith("{") for l in lines[i + 1 : i + 3]):
            idx = i
            break
    if idx is None:
        return False
    for line in reversed(lines[:idx]):
        st = line.strip()
        if not st.startswith("#"):
            continue
        m = re.match(r"#\s*(if|ifdef|ifndef|elif)\s+(.*)", st)
        if not m:
            continue
        cond = m.group(2)
        linuxish = re.search(r"\b__(linux|APPLE|unix)__\b", cond)
        win = re.search(r"_WIN32", cond)
        # `#if defined(__linux__) && !defined(_WIN32)` (src/window_wayland.c line 1) IS the
        # Windows-excluded case; the first version rejected it merely because _WIN32 appeared.
        if linuxish and (not win or "!defined(_WIN32)" in cond or "!_WIN32" in cond):
            return True
        if re.search(r"\b_WIN32\b", cond) and m.group(1) == "ifndef":
            return True
    return False


def object_globals(objs: list[Path]) -> set[str]:
    """nm only takes so many arguments -- Windows caps the command line, so walk in batches."""
    out: set[str] = set()
    for i in range(0, len(objs), 300):
        batch = objs[i:i + 300]
        r = subprocess.run([str(NM), "-g", "--defined-only", *[str(o) for o in batch]],
                           capture_output=True, text=True, errors="replace")
        for name in re.findall(r"^[0-9a-fA-F]+\s+[TDBRWG]\s+(\S+)$", r.stdout, re.M):
            # COFF gives a weak definition the name ".weak.<symbol>.<other>" and reports the plain
            # name as an undefined w. agent_build_child_agent is defined that way on purpose (a weak
            # hook the engine may override), and it resolves fine at link time.
            m = re.match(r"^\.weak\.([^.]+)\.", name)
            out.add(m.group(1) if m else name)
    return out


def main() -> int:
    fix = "--fix" in sys.argv
    want_json = None
    if "--json" in sys.argv:
        want_json = Path(sys.argv[sys.argv.index("--json") + 1])

    sources = [p for p in (REPO / "src").rglob("*.c")]
    # Every object in the link, not just src/: scripts/localize_dupes.py deliberately makes the
    # losing copy of a duplicate definition local (t) so the link can carry every translation unit,
    # and the keeper usually lives in lib/ -- src/agent/threat_patterns.o is localized while
    # lib/libthreatpatterns/threat_patterns.o keeps the global symbol. Scanning only src/ reported
    # that as five missing symbols.
    all_objs = list((REPO / "src").rglob("*.o")) + list((REPO / "lib").rglob("*.o"))
    exported = object_globals(all_objs)
    print(f"sources: {len(sources)}  objects: {len(all_objs)}  exported symbols: {len(exported)}")

    own_defs = object_defs_map([p.with_suffix(".o") for p in sources if p.with_suffix(".o").exists()])
    broken: dict[str, list[str]] = {}
    missing_obj: list[str] = []
    for src in sources:
        obj = src.with_suffix(".o")
        if not obj.exists():
            missing_obj.append(str(src.relative_to(REPO)))
            continue
        defs = source_defs(src)
        if not defs:
            continue
        # cheap pre-filter: does this object export anything at all?
        own = own_defs.get(str(obj), set())
        # No fallback to the union of all objects: a stub in another translation unit can satisfy the
        # name for the file that owns the implementation, and that is precisely the state
        # tools_init_all / i18n_t / slermes_home were in while this check reported zero.
        # This tree neutralises a shadow by macro-renaming it at compile time
        # (#define <name> stub_shadowed_<name>). The source still spells the original name, so an
        # object holding only the renamed symbol is correct.
        lack = [d for d in defs if d not in own and ("stub_shadowed_" + d) not in own]
        if lack:
            # Two kinds of honest absence must not be reported as a stale object:
            #   - `main`: this tree keeps alternative entry points (src/main.c, main_desktop.c,
            #     desktop_gui.c, agent/test_omarchy_usage_main.c) that the product link does not use
            #     as its entry;
            #   - a definition inside a platform block the Windows build compiles out, e.g.
            #     src/tools/terminal.c's run_command_pty under `#if defined(__linux__) || __APPLE__`.
            kept = []
            for d in lack:
                if d == "main" or platform_excluded(src, d):
                    continue
                kept.append(d)
            if kept:
                broken[str(src.relative_to(REPO))] = kept

    print(f"sources without an object: {len(missing_obj)}")
    print(f"objects that do not define what their source defines: {len(broken)}")
    for path, lack in sorted(broken.items(), key=lambda kv: -len(kv[1]))[:12]:
        print(f"  {path:52} {len(lack):3} missing, e.g. {', '.join(lack[:4])}")
    if want_json:
        want_json.write_text(json.dumps(broken, indent=1), encoding="utf-8")
        print(f"wrote {want_json}")
    if fix:
        n = 0
        for path in broken:
            os.utime(REPO / path, None)
            n += 1
        print(f"touched {n} source(s); run make to rebuild them")
    if "--fail-on-drift" in sys.argv and (broken or missing_obj):
        # For the build: an object that does not define what its source defines is a stale object,
        # and a link over it produces a binary that does not contain the code the tree says it does.
        # tools_init_all, i18n_t, delegate_tool's 13 functions and the cron web wrappers were all in
        # this state while the link reported success, so make this fatal.
        print(f"FAIL: {len(broken)} object(s) do not define their source's symbols, "
              f"{len(missing_obj)} source(s) have no object "
              f"(rebuild them: python scripts/verify_objects.py --fix)", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
