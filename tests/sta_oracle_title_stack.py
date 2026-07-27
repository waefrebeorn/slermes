#!/usr/bin/env python3
"""
sta_oracle_title_stack.py — Python oracle for the title-generation stack:
utils.py (is_truthy_value), hermes_state.py (SessionDB.sanitize_title),
agent/skill_commands.py (describe_skill_invocation,
extract_user_instruction_from_skill_message), and
agent/title_generator.py (_summarize_user_message).

Fixture ops (one per line; \\e -> ESC, \\n -> newline, \\x1e -> RS in text):
  truthy <value|__none__> <default:true|false>
  sanitize <text>
  describe <text>
  extract <text>
  summarize <text>
Output: one compact JSON object per line (ensure_ascii=True).
"""
import json
import os
import pwd
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)
# Upstream Python root candidates. The oracle runner overrides HOME to a temp
# dir, so resolve the real home via pwd (env-independent), plus the sibling
# layout ($SLERMES_SRC / C-tree parent) per repo convention.
_REAL_HOME = pwd.getpwuid(os.getuid()).pw_dir
for _cand in (os.environ.get("SLERMES_SRC") or "",
              os.path.dirname(_ROOT),
              os.path.join(_REAL_HOME, ".hermes", "hermes-agent")):
    if _cand and os.path.isfile(os.path.join(_cand, "utils.py")):
        if _cand not in sys.path:
            sys.path.append(_cand)
        break

from utils import is_truthy_value
from hermes_state import SessionDB
from agent.skill_commands import (
    describe_skill_invocation,
    extract_user_instruction_from_skill_message,
)
from agent.title_generator import _summarize_user_message


def decode(text):
    return (text.replace("\\e", "\x1b")
                .replace("\\n", "\n")
                .replace("\\x1e", "\x1e"))


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"),
                                ensure_ascii=False) + "\n")


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_title_stack.py <cases.in>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as fp:
        for raw in fp:
            line = raw.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            op, _, rest = line.partition(" ")
            if op == "truthy":
                val, _, dflt = rest.partition(" ")
                v = None if val == "__none__" else decode(val)
                emit({"op": "truthy",
                      "out": is_truthy_value(v, default=(dflt == "true"))})
            elif op == "sanitize":
                text = decode(rest)
                try:
                    emit({"op": "sanitize", "out": SessionDB.sanitize_title(text),
                          "error": None})
                except ValueError:
                    emit({"op": "sanitize", "out": None, "error": "too_long"})
            elif op == "describe":
                emit({"op": "describe",
                      "out": describe_skill_invocation(decode(rest))})
            elif op == "extract":
                emit({"op": "extract",
                      "out": extract_user_instruction_from_skill_message(decode(rest))})
            elif op == "summarize":
                emit({"op": "summarize",
                      "out": _summarize_user_message(decode(rest))})
    return 0


if __name__ == "__main__":
    sys.exit(main())
