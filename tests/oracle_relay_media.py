#!/usr/bin/env python3
"""Oracle for gateway/relay/media.py _content_disposition_filename + media_base_url.

Harness emits {"tag","in","out"}: "in" is the raw input, "out" is the C result.
This oracle ast-extracts the LIVE defs (import chain is poisonous) and runs them
on "in", comparing to "out".
"""
import ast
import email
import email.message
import email.utils
import json
import sys
import unicodedata

sys.stdin.reconfigure(encoding="utf-8")
HERMES = "C:/Users/eman5/hermes-agent"


def extract(path, names):
    tree = ast.parse(open(path, encoding="utf-8").read())
    nodes = []
    for n in tree.body:
        if isinstance(n, (ast.FunctionDef,)):
            if n.name in names:
                nodes.append(n)
        elif isinstance(n, (ast.Assign, ast.AnnAssign)):
            tg = getattr(n, "target", None)
            if tg is None and getattr(n, "targets", None):
                tg = n.targets[0]
            if isinstance(tg, ast.Name) and tg.id in names:
                nodes.append(n)
    ns = {"unicodedata": unicodedata, "email": email,
          "email.message": email.message, "email.utils": email.utils}
    for n in nodes:
        exec(compile(ast.Module(body=[n]), "<x>", "exec"), ns)
    return ns


# clean_attachment_filename + _MAX_FILENAME_CHARS (dependency of the function under test)
dep = extract(f"{HERMES}/gateway/platforms/event.py",
              {"clean_attachment_filename", "_MAX_FILENAME_CHARS"})
# functions under test
mod = extract(f"{HERMES}/gateway/relay/media.py",
              {"_content_disposition_filename", "media_base_url"})
for k, v in dep.items():
    mod[k] = v

cdf = mod["_content_disposition_filename"]
mbu = mod["media_base_url"]

passes = 0
fails = []
total = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        case = json.loads(line)
    except json.JSONDecodeError:
        continue
    tag = case.get("tag")
    input_val = case.get("in")
    if input_val is None:
        input_val = ""
    func = {"cdf": cdf, "mbu": mbu}.get(tag)
    if func is None:
        continue
    try:
        out = func(input_val)
    except Exception as e:  # noqa
        out = "<RAISE:%s>" % type(e).__name__
    total += 1
    if out == case.get("out"):
        passes += 1
    else:
        fails.append((tag, input_val, out, case.get("out")))

print("pass/total: %d/%d" % (passes, total))
for f in fails[:12]:
    print("FAIL %s in=%r -> py=%r c=%r" % f)
if fails:
    print("ORACLE relay_media: %d cases, %d mismatches" % (total, len(fails)))
    sys.exit(1)
print("ORACLE relay_media: %d cases, 0 mismatches" % total)
