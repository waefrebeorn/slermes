#!/usr/bin/env python3
"""
god_header_eliminate2.py — second pass for files that KEPT hermes.h because
they use json_t/json_node_t (need hermes_json.h) or F_OK/X_OK (need <unistd.h>).

Rather than the god header, we inject the minimal include and re-test.
If they still fail, restore god header.
"""
import subprocess, os

ROOT = "/home/wubu/hermes-agent-dev/slermes"
os.chdir(ROOT)

def make_obj(src):
    obj = src[:-2] + ".o"
    r = subprocess.run(["make", obj], capture_output=True, text=True)
    return r.returncode == 0, r.stdout + r.stderr

def read(p):
    with open(p) as f: return f.read()
def write(p, s):
    with open(p, "w") as f: f.write(s)

# files from first pass that KEPT: (path, minimal include to try)
targets = {
 "src/cli/port_tools_schema_sanitizer.c": '#include "hermes_json.h"',
 "src/cli/port_gateway_platforms_yuanbao_sticker.c": '#include "hermes_json.h"',
 "src/cli/port_tools_environments_file_sync.c": '#include "hermes_json.h"',
 "src/cli/port_tools_environments_modal_utils.c": '#include "hermes_json.h"',
 "src/cli/port_gateway_platforms_wecom_callback.c": '#include "hermes_json.h"',
 "src/cli/port_tools_environments_managed_modal.c": '#include "hermes_json.h"',
 "src/cli/port_tools_yuanbao_tools.c": '#include "hermes_json.h"',
 "src/cli/port_tools_voice_mode.c": '#include <unistd.h>',
}

fixed, still_kept = [], []
for p, inc in targets.items():
    if not os.path.exists(p):
        print("missing", p); continue
    src = read(p)
    if '#include "hermes.h"' not in src:
        # already handled? skip
        print("already no hermes.h:", p); continue
    # remove god header, add minimal include after the last existing #include or at top
    new = src.replace('#include "hermes.h"\n', '', 1)
    # insert minimal include near top (after first include line)
    lines = new.splitlines(keepends=True)
    out = []
    inserted = False
    for i, l in enumerate(lines):
        out.append(l)
        if not inserted and (l.startswith('#include')):
            out.append(inc + "\n")
            inserted = True
    if not inserted:
        out.insert(0, inc + "\n")
    write(p, "".join(out))
    ok, log = make_obj(p)
    if ok:
        fixed.append(p); print("  FIXED(minimal) ", p, "->", inc)
    else:
        write(p, src)  # restore fully
        still_kept.append(p)
        errs = [l for l in log.splitlines() if "error:" in l][:2]
        print("  STILL KEPT    ", p, "->", "; ".join(errs))

print(f"\nFIXED={len(fixed)} STILL_KEPT={len(still_kept)}")
