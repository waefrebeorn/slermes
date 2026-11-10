#!/usr/bin/env python3
"""Oracle for the relay-media / plugin-label / credential-generation helpers
port (src/cli/port_media_labels.c).

Harness (t_port_media_labels.c) emits per family:
  burl:  {family:"burl", in:<str|null>, out:<str>}   -> gateway/relay/media.py:media_base_url
  url:   {family:"url",  in:<str|null>, out:<0|1>}   -> RelayMediaClient.is_relay_media_url
  label: {family:"label", emoji:<str|null>, label:<str|null>, out:<str>}
                                                        -> hermes_cli/platforms.py:_plugin_label
  gen:   {family:"gen",  in:<str>, out:<64-hex>}     -> hermes_cli/browser_credential.py:generation_of

Ast-extract the live defs (module fn, class method, two fns); seed hashlib for
generation_of and a SimpleNamespace {emoji,label} for _plugin_label.
"""
import ast
import hashlib
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def grab(path, names, method_of=None, seeds=None):
    src = open(path, encoding="utf-8").read()
    ns = {"Any": object, "Optional": object, "hashlib": hashlib}
    if seeds:
        ns.update(seeds)
    wanted = set(names)
    body = []
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name in wanted:
            body.append(n)
        if method_of and isinstance(n, ast.ClassDef) and n.name == method_of:
            for m in n.body:
                if isinstance(m, ast.FunctionDef) and m.name in wanted:
                    body.append(m)
    exec(compile(ast.Module(body=body, type_ignores=[]), path, "exec"), ns)
    return ns


def main() -> int:
    media = grab("C:/Users/eman5/hermes-agent/gateway/relay/media.py",
                 ["media_base_url", "is_relay_media_url"],
                 method_of="RelayMediaClient")
    base_url = media["media_base_url"]

    class RelayClient:
        is_relay_media_url = media["is_relay_media_url"]

    platforms = grab("C:/Users/eman5/hermes-agent/hermes_cli/platforms.py",
                     ["_plugin_label"])
    plugin_label = platforms["_plugin_label"]

    creds = grab("C:/Users/eman5/hermes-agent/hermes_cli/browser_credential.py",
                 ["generation_of"])
    generation = creds["generation_of"]

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
        if fam == "burl":
            want = base_url(c.get("in"))
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH burl in={c.get('in')!r} C={got!r} PY={want!r}")
        elif fam == "url":
            want = 1 if RelayClient.is_relay_media_url(_self(), c.get("in")) else 0
            got = int(c.get("out"))
            if want != got:
                mism += 1
                print(f"MISMATCH url in={c.get('in')!r} C={got} PY={want}")
        elif fam == "label":
            entry = types.SimpleNamespace(emoji=c.get("emoji"), label=c.get("label"))
            want = plugin_label(entry)
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH label emoji={c.get('emoji')!r} label={c.get('label')!r} "
                      f"C={got!r} PY={want!r}")
        elif fam == "gen":
            want = generation(c.get("in") or "")
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH gen in={c.get('in')!r} C={got} PY={want}")
    if total == 0:
        print("ORACLE media_labels: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE media_labels: {total} cases, {mism} mismatches")
    return 1 if mism else 0


class _self:
    pass


if __name__ == "__main__":
    sys.exit(main())
