#!/usr/bin/env python3
"""
port_scaffold.py — Slermes pure-function port scaffold generator.

Builds the five artifacts a verified pure-function port needs, with every
hard-won discipline baked in so a new port starts from a correct skeleton
instead of from a blank buffer:

  src/cli/port_<basename>.c     exported C fn, PoP annotation naming it,
                                original live-Python def embedded as a comment
  include/port_<basename>.h     guard + prototype (BOM-free, LF)
  tests/t_port_<basename>.c     oracle harness skeleton (jstr + loop + tag)
  tests/oracle_<basename>.py    ast-extract oracle skeleton (namespace seed +
                                ``N cases, 0 mismatches`` gate protocol)
  build/objects.mk              registers port_<basename>.o (CRLF-safe, dedup)

It also locates the matching cli_gaps.c stub line and prints the exact
compile + gate commands. It never compiles or commits — the human/agent wires
the signature and cases, which is the part that still needs judgment.

Usage:
  python tests/port_scaffold.py --module tools/idle_watch.py --func next_check_in \
      --cname idw_next_check_in [--basename idle_watch] [--tag nci]

  --module  Python file relative to hermes-agent (e.g. tools/foo.py)
  --func    the pure function being ported
  --cname   the EXPORTED C function name (MUST be what you name the def; the
            strict parity scorer keys group1 of the PoP annotation to this and
            requires it to be a real C def, so it is the identity)
  --basename  port file basename (default: derived from --cname prefix or module)
  --tag     JSON tag for the harness lines (default: short --cname)
"""

import argparse
import re
import sys
from pathlib import Path

HERMES = Path("C:/Users/eman5/hermes-agent")
SLERMES = Path("C:/Users/eman5/slermes")


def live_def(module: str, func: str):
    """Return (source_text_of_def, file_path) ast-extracted verbatim, or None."""
    p = HERMES / module
    if not p.is_file():
        return None, p
    import ast
    src = p.read_text(encoding="utf-8", errors="replace")
    try:
        tree = ast.parse(src)
    except SyntaxError:
        return None, p
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == func:
            seg = ast.get_source_segment(src, node)
            if seg:
                return seg, p
    return None, p


def short_tag(cname: str) -> str:
    toks = [t for t in re.split(r"[_]+", cname) if t]
    inner = "".join(t[:2] for t in toks)
    return inner[:4] or "out"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--module", required=True)
    ap.add_argument("--func", required=True)
    ap.add_argument("--cname", required=True, help="exported C fn name = PoP identity")
    ap.add_argument("--basename", default=None)
    ap.add_argument("--tag", default=None)
    ap.add_argument("--return-type", default="int", help="C return type in prototype")
    ap.add_argument("--args", default="void", help="C param list in prototype")
    args = ap.parse_args()

    base = args.basename or args.cname
    if base.startswith(("port_", "t_port_", "oracle_")):
        base = re.sub(r"^(port_|t_port_|oracle_)", "", base)
    tag = args.tag or short_tag(args.cname)

    py_src, py_path = live_def(args.module, args.func)
    if py_src is None:
        print(f"WARN: could not ast-extract {args.module}::{args.func} (path={py_path}); "
              "embedding path reference only.")

    # ---------- port_<base>.c ----------
    poc = SLERMES / "src" / "cli" / f"port_{base}.c"
    embed = f"#   live python ({args.module}:{args.func}):\n#\n" + \
            "\n".join("#   " + ln for ln in (py_src or "").splitlines()) + "\n"
    c_src = (
        f"/*\n"
        f" * port_{base}.c — Port of {args.module}::{args.func} (pure).\n"
        f" *\n{embed}"
        f" *\n"
        f" * Structural note: {args.cname} is BOTH the exported C def name and the\n"
        f" * first token of the PoP annotation, so parity_reality's strict scorer\n"
        f" * maps ( {args.module}, {args.func} ) -> {args.cname} and finds the def.\n"
        f" */\n"
        f"\n"
        f'#include "port_{base}.h"\n'
        f"\n"
        f"/* PoP: {args.cname} @ {args.module}:{args.func} */\n"
        f"{args.return_type} {args.cname}({args.args})\n"
        f"{{\n"
        f"    /* TODO: implement against the live def above. */\n"
        f"    return 0;\n"
        f"}}\n"
    )
    poc.parent.mkdir(parents=True, exist_ok=True)
    poc.write_text(c_src, encoding="utf-8", newline="\n")

    # ---------- include/port_<base>.h ----------
    guard = f"SLERMES_PORT_{base.upper()}_H"
    h_src = (
        f"#ifndef {guard}\n"
        f"#define {guard}\n"
        f"\n"
        f"/* {module_last(args.module)}.{args.func} — see the .c for the live-def\n"
        f" * contract. {args.return_type} {args.cname}({pretty_args(args.args)});\n"
        f" */\n"
        f"{args.return_type} {args.cname}({args.args});\n"
        f"\n"
        f"#endif /* {guard} */\n"
    )
    (SLERMES / "include" / f"port_{base}.h").write_text(h_src, encoding="utf-8", newline="\n")

    # ---------- tests/t_port_<base>.c ----------
    t_src = (
        f"/* t_port_{base}.c — oracle harness for {args.func}.\n"
        f" * Emits {{\"tag\":\"{tag}\",\"case\":N,...\"out\":<the C result>}} lines.\n"
        f" * Replace the skeleton below with struct-array cases + one loop.\n"
        f" */\n"
        f'#include "port_{base}.h"\n'
        f"\n"
        f"#include <stdio.h>\n"
        f"\n"
        f"static void jstr(const char *s)\n"
        f"{{\n"
        f"    if (!s) {{ fputs(\"null\", stdout); return; }}\n"
        f"    fputc('\"', stdout);\n"
        f"    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {{\n"
        f"        switch (*p) {{\n"
        f"        case '\"': fputs(\"\\\\\\\"\", stdout); break;\n"
        f"        case '\\\\': fputs(\"\\\\\\\\\", stdout); break;\n"
        f"        case '\\n': fputs(\"\\\\n\", stdout); break;\n"
        f"        case '\\r': fputs(\"\\\\r\", stdout); break;\n"
        f"        case '\\t': fputs(\"\\\\t\", stdout); break;\n"
        f"        default: fputc((char)*p, stdout); break;\n"
        f"        }}\n"
        f"    }}\n"
        f"    fputc('\"', stdout);\n"
        f"}}\n"
        f"\n"
        f"int main(void)\n"
        f"{{\n"
        f"    setvbuf(stdout, NULL, _IONBF, 0);\n"
        f"    /* TODO: struct-array of cases; call {args.cname}(...). */\n"
        f'    printf("{{\\"tag\\":\\"{tag}\\",\\"case\\":0,\\"out\\":0}}\\n");\n'
        f"    return 0;\n"
        f"}}\n"
    )
    (SLERMES / "tests" / f"t_port_{base}.c").write_text(t_src, encoding="utf-8", newline="\n")

    # ---------- tests/oracle_<base>.py ----------
    o_src = (
        f"#!/usr/bin/env python3\n"
        f'"""Oracle for {args.module}::{args.func}.\n'
        f"\n"
        f"Ast-extract the LIVE `{args.func}` and compare against the harness\n"
        f"emitted `{tag}` lines. Seed every free var below (namespace globals +\n"
        f"sys.modules fakes + ns[\\\"logger\\\"] no-op) the live def references.\n"
        f'"""\n'
        f"import ast\n"
        f"import json\n"
        f"import sys\n"
        f"\n"
        f'SRC = "C:/Users/eman5/hermes-agent/{args.module}"\n'
        f"\n"
        f"\n"
        f"def main() -> int:\n"
        f"    src = open(SRC, encoding=\"utf-8\").read()\n"
        f"    ns = {{}}  # TODO: ns[\\\"Path\\\"]=Path; ns[\\\"math\\\"]=__import__(\\\"math\\\"); ...\n"
        f"    for n in ast.parse(src).body:\n"
        f"        if isinstance(n, ast.FunctionDef) and n.name == \"{args.func}\":\n"
        f"            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, \"exec\"), ns)\n"
        f"        # also ast-extract referenced classes/constants the live def uses\n"
        f"    FN = ns[\"{args.func}\"]\n"
        f"\n"
        f"    cases = []\n"
        f"    for line in sys.stdin.read().splitlines():\n"
        f"        line = line.strip()\n"
        f"        if not line:\n"
        f"            continue\n"
        f"        try:\n"
        f"            cases.append(json.loads(line))\n"
        f"        except Exception:\n"
        f"            pass\n"
        f"    if not cases:\n"
        f'        print("ORACLE {base}: 0 cases (link failure in disguise)")\n'
        f"        return 1\n"
        f"    mism = 0\n"
        f"    total = 0\n"
        f"    for c in cases:\n"
        f'        if c.get("tag") != "{tag}":\n'
        f"            continue\n"
        f"        total += 1\n"
        f"        # TODO: map c[...] to the live args\n"
        f"        want = FN()\n"
        f'        if want != c["out"]:\n'
        f"            mism += 1\n"
        f"            print(f\"MISMATCH {{c.get('case')}} C={{c.get('out')!r}} PY={{want!r}}\")\n"
        f"    print(f\"ORACLE {base}: {{total}} cases, {{mism}} mismatches\")\n"
        f"    return 1 if mism else 0\n"
        f"\n"
        f"\n"
        f'if __name__ == "__main__":\n'
        f"    sys.exit(main())\n"
    )
    (SLERMES / "tests" / f"oracle_{base}.py").write_text(o_src, encoding="utf-8", newline="\n")

    # ---------- build/objects.mk registration (CRLF-safe, dedup) ----------
    oname = f"port_{base}.o"
    mk = SLERMES / "build" / "objects.mk"
    data = mk.read_text(encoding="utf-8", newline="")
    need_nl = "\n" not in data
    reg = f"src/cli/{oname}"
    in_obj = re.search(r"(\nCLI_OBJ =[^\n]*)", data)
    anchor = " src/cli/port_fast_mode.o"
    already = re.search(rf"src/cli/{re.escape(oname)}(?=\\? \s|\\\\|$)", data)
    if already:
        print("objects.mk: already registered; leaving untouched.")
    else:
        if anchor in data:
            data = data.replace(anchor, anchor + " " + reg, 1)
        else:
            # fall back to appending before the trailing backslash of CLI_OBJ line
            m = re.search(r"(?s)(\nCLI_OBJ =.*?)(\\\n)", data)
            data = data[: m.start()] + m.group(1) + " " + reg + " \\\n" + data[m.end():]
        if need_nl:
            data = data.rstrip("\n") + "\n"
        mk.write_text(data, encoding="utf-8", newline="")
        print(f"objects.mk: inserted {reg} (CRLF-safe).")

    # ---------- locate cli_gaps.c stub ----------
    cg = SLERMES / "src" / "cli" / "cli_gaps.c"
    if cg.is_file():
        cg_src = cg.read_text(encoding="utf-8", errors="replace")
        for i, ln in enumerate(cg_src.splitlines(), 1):
            if args.func in ln and ("PoP:" in ln or args.func in ln):
                if re.search(rf"\b{re.escape(args.func)}\b", ln):
                    print(f"cli_gaps.c:{i}  {ln.strip()}")
        else:
            pass

    print(f"\nScaffolded port `{base}` for {args.module}::{args.func}:")
    for rel in (f"src/cli/port_{base}.c", f"include/port_{base}.h",
                f"tests/t_port_{base}.c", f"tests/oracle_{base}.py"):
        print(f"  build/{rel}")
    print("\nNext (wire signature, cases, seed namespace), then:\n"
          f'  gcc -w -O2 -std=gnu11 -D_GNU_SOURCE -I include -I src -I lib/libjson '
          f'-I lib/libregex -c src/cli/port_{base}.c -o src/cli/port_{base}.o\n'
          f"  MINIMAL_OBJS=\"src/cli/port_{base}.o lib/libjson/json.o [deps]\" \\\n"
          f"    bash tests/run_one_oracle.sh {base} oracle_{base}.py\n"
          f"  # require ORACLE {base}: N cases, 0 mismatches, GATE_EXIT=0\n"
          f"  # then delete the cli_gaps.c stub, `make -j8 slermes`, verify --version, commit")
    return 0


def module_last(module: str) -> str:
    return "/".join(module.split("/")[-2:])


def pretty_args(args: str) -> str:
    return args or "void"


if __name__ == "__main__":
    sys.exit(main())
