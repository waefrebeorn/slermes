#!/usr/bin/env python3
"""Oracle for agent/anthropic_message_convert.py block-shaping helpers
(port_anthropic_message_convert.c). The live module cannot be imported in this
interpreter (hermes_yaml pulls uninstalled ruamel at load), so ast-extract the
closure of pure helpers verbatim and seed the module constants they reference
(_EMPTY_TEXT_PLACEHOLDER, _REPLAY_SANITIZERS built from the _replay_* fns, the
frozenset _THINKING_TYPES/_CACHEABLE default fns). A fake tools.vision_tools_image_prep
is injected on sys.modules for the inline import inside _image_block_from_openai_url.

Harness families (emit-input rewrite of the old out-only harness):
  carry:  {family, case, out_json, block_json, copy, out}   _carry_cache_control
  split:  {family, case, blocks, out}                        _split_blank_text_blocks
          (C emits {"kept","relocated","dropped"}; from live's tuple)
  safe_text:{family, case, text, out}                        _safe_text
  image:  {family, case, url, out}                           _image_block_from_openai_url
  replay: {family, case, block, out}                         _sanitize_replay_block
"""
import ast
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/anthropic_message_convert.py"


def _canon(x):
    if x is None:
        return ("null",)
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, (list, tuple)):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def _module_ns(names):
    """Ast-extract the named FunctionDefs + Assigns (+ fns they reference) into a
    single namespace in source order (Assigns follow their referenced defs)."""
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    wanted = set(names)
    all_names = set(names)
    # transitive FunctionDef closure over the wanted names
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in wanted:
            for sub in ast.walk(n):
                if isinstance(sub, ast.Name) and isinstance(sub.ctx, ast.Load):
                    all_names.add(sub.id)
    ns = {"Any": object, "Optional": object, "copy": __import__("copy"),
          "re": __import__("re")}
    # seed fakes for the inline import before exec
    body = []
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in all_names:
            body.append(n)
        elif isinstance(n, ast.Assign) and getattr(n.targets[0], "id", "") in all_names:
            # emit the Assign AFTER its referenced defs (source already orders them)
            body.append(n)
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)
    return ns


def main() -> int:
    # fake tools.vision_tools_image_prep for the inline import
    _SUPPORTED = frozenset({"image/jpeg", "image/png", "image/gif", "image/webp"})

    def unsupported_inline_image_media_type(url):
        header = str(url).partition(",")[0].lower()
        if not header.startswith("data:image/"):
            return None
        subtype = header[len("data:image/"):].split(";", 1)[0].strip() or "unknown"
        media_type = "image/jpeg" if subtype == "jpg" else "image/%s" % subtype
        return None if media_type in _SUPPORTED else media_type

    def rasterize_svg_data_url(url):
        return None

    prep = types.ModuleType("tools.vision_tools_image_prep")
    prep.unsupported_inline_image_media_type = unsupported_inline_image_media_type
    prep.rasterize_svg_data_url = rasterize_svg_data_url
    sys.modules.setdefault("tools", types.ModuleType("tools"))
    sys.modules["tools.vision_tools_image_prep"] = prep
    sys.modules["tools"] = sys.modules.get("tools")

    ns = _module_ns(
        ["_carry_cache_control", "_split_blank_text_blocks",
         "_image_block_from_openai_url", "_safe_text", "_sanitize_replay_block",
         "_replay_text", "_replay_thinking", "_replay_redacted_thinking",
         "_replay_tool_use", "_replay_image", "_tool_use_block",
         "_text_block_with_citations", "_text_block", "_is_blank_text_block",
         "_block_type", "_cache_control_of", "_sanitize_tool_id",
         "_REPLAY_SANITIZERS", "_EMPTY_TEXT_PLACEHOLDER"])

    def carry(out_json, block_json, copy_marker):
        return ns["_carry_cache_control"](out_json, block_json, copy=copy_marker)

    def sanitize(block):
        return ns["_sanitize_replay_block"](block)

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fam = c.get("family")
        if fam == "carry":
            want = carry(c.get("out_json"), c.get("block_json"), c.get("copy"))
            got = c.get("out")
            if _canon(want) != _canon(got):
                mism += 1
                print(f"MISMATCH carry {c.get('case')} C={got} PY={want}")
        elif fam == "split":
            kept, relocated, dropped = ns["_split_blank_text_blocks"](c.get("blocks") or [])
            want = {"kept": kept, "relocated": relocated, "dropped": dropped}
            got = c.get("out")
            if _canon(want) != _canon(got):
                mism += 1
                print(f"MISMATCH split {c.get('case')} C={got} PY={want}")
        elif fam == "safe_text":
            want = ns["_safe_text"](c.get("text"))
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH safe_text {c.get('case')} text={c.get('text')!r} C={got!r} PY={want!r}")
        elif fam == "image":
            want = ns["_image_block_from_openai_url"](c.get("url") or "")
            got = c.get("out")
            if _canon(want) != _canon(got):
                mism += 1
                print(f"MISMATCH image {c.get('case')} url={c.get('url')!r}\n"
                      f"  C={got}\n  PY={want}")
        elif fam == "replay":
            want = sanitize(c.get("block"))
            got = c.get("out")
            if _canon(want) != _canon(got):
                mism += 1
                print(f"MISMATCH replay {c.get('case')} C={got}\n  PY={want}")
    if total == 0:
        print("ORACLE anthropic_message_convert: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE anthropic_message_convert: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())

