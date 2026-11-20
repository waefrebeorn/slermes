#!/usr/bin/env python3
"""Generate tests/fixtures/turns_flow4.jsonl — inputs for the turns.py
REMAINDER-beyond-flow3 FLOW oracle (sentinel codec + skill-scaffold describer +
auto-load registry).

The oracle computes want from the LIVE quarry defs at gate time; this script
only emits {func, in} inputs. Representation contract (shared with C + oracle):
None == JSON null; plain JSON numbers are Python ints; bools are bools. A Python
tuple return is transported as a JSON array. The auto-load registry is carried
in `in.state.scaffolds` (ordered list of strings) and echoed as out.state.

`oracle.drive` is called on every case so a typo fails loudly at generation.
"""
import copy
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import oracle_turns_flow4 as oracle  # noqa: E402

OUT = HERE / "fixtures" / "turns_flow4.jsonl"
CASES = []

PFX = oracle.PFX
AL = oracle.AL_PREFIX
AL_SUFFIX = oracle.AL_SUFFIX
SK = oracle.SKILL_INV_PREFIX
BUNDLE = oracle.BUNDLE_MARKER
BUSER = oracle.BUNDLE_USER_INSTR
BFIRST = oracle.BUNDLE_FIRST_BLOCK
SMARK = oracle.SINGLE_MARKER
SINSTR = oracle.SINGLE_INSTR
RNOTE = oracle.RUNTIME_NOTE
DEND = oracle.DIR_NOTE_END


def add(func, args, scaffolds=None):
    inp = {"args": copy.deepcopy(args)}
    if scaffolds is not None:
        inp["state"] = {"scaffolds": copy.deepcopy(scaffolds)}
    oracle.drive(func, inp)  # fail loudly on a typo / bad domain
    CASES.append({"func": func, "in": inp})


def al_header(name="work"):
    return AL + name + AL_SUFFIX


def al_payload(name="work", typed="fix the bug"):
    return al_header(name) + "\n\nbody of skill\n" + DEND + "\n\n" + typed


def single_payload(name="work", instruction="fix it"):
    return (SK + '"' + name + '"' + " — invoked.\n"
            + SMARK + "\nbody\n" + SINSTR + instruction + RNOTE + " details")


def bundle_payload(name="/a /b", instruction="do both"):
    return (SK + '"' + name + '"' + BUNDLE + " loaded\n"
            + BUSER + instruction + BFIRST + "work" + "\nbody")


# ── decode_content (8) ──
add("decode_content", {"content": "plain text"})
add("decode_content", {"content": None})
add("decode_content", {"content": {"a": 1}})
add("decode_content", {"content": PFX + '{"a": 1, "b": [2, 3]}'})
add("decode_content", {"content": PFX + '["x", "y"]'})
add("decode_content", {"content": PFX + '"hello"'})
add("decode_content", {"content": PFX + '42'})
add("decode_content", {"content": PFX + 'this is not json'})

# ── extract_user_instruction_from_skill_message (7) ──
add("extract_user_instruction_from_skill_message", {"content": 1234})
add("extract_user_instruction_from_skill_message", {"content": "just a normal request"})
add("extract_user_instruction_from_skill_message", {"content": al_payload(typed="  ship it  ")})
add("extract_user_instruction_from_skill_message", {"content": al_header("work")},
    scaffolds=[])
add("extract_user_instruction_from_skill_message", {"content": bundle_payload()})
add("extract_user_instruction_from_skill_message", {"content": single_payload()})
add("extract_user_instruction_from_skill_message", {"content": SK + '"work" alone without marker'})

# ── _describe_auto_loaded_skill_turn (6) ──
add("_describe_auto_loaded_skill_turn", {"content": "not a scaffold"})
add("_describe_auto_loaded_skill_turn", {"content": al_payload(typed="fix   the  bug")})
add("_describe_auto_loaded_skill_turn", {"content": al_header("work")})
add("_describe_auto_loaded_skill_turn", {"content": al_header("")})
add("_describe_auto_loaded_skill_turn",
    {"content": al_header("x") + "\n" + DEND + "\n[Skill config (\nkey=1\n]\nfinal ask here"})
add("_describe_auto_loaded_skill_turn",
    {"content": al_header("x") + "\n" + DEND + "\n[This skill has supporting files see /scripts/foo.js`).\nreal request"})

# ── describe_skill_invocation (8) ──
add("describe_skill_invocation", {"content": 99})
add("describe_skill_invocation", {"content": "ordinary text"})
add("describe_skill_invocation", {"content": single_payload(name="work", instruction="fix the title leak")})
add("describe_skill_invocation", {"content": bundle_payload(), "separator": " "})
add("describe_skill_invocation", {"content": SK + '"work" bare invocation'})
add("describe_skill_invocation", {"content": al_payload(typed="ship it")})
add("describe_skill_invocation",
    {"content": single_payload(name="work", instruction="left\u001eright extra")})
add("describe_skill_invocation", {"content": SK + '"work" no marker at all'})

# ── _cut_after (7) ──
add("_cut_after", {"message": "before [START] pick me [STOP] after", "marker": "[START]",
                   "stop_marker": "[STOP]", "find": "find"})
add("_cut_after", {"message": "a [M] tail no stop", "marker": "[M]",
                   "stop_marker": "[END]", "find": "find"})
add("_cut_after", {"message": "nothing here", "marker": "[M]",
                   "stop_marker": "[END]", "find": "find"})
add("_cut_after", {"message": "x [M] first [S] y [M] second [S] z", "marker": "[M]",
                   "stop_marker": "[S]", "find": "rfind"})
add("_cut_after", {"message": "x [M] first [M] second [S] z", "marker": "[M]",
                   "stop_marker": "[S]", "find": "find"})
add("_cut_after", {"message": "a [M]   [S] b", "marker": "[M]",
                   "stop_marker": "[S]", "find": "find"})
add("_cut_after", {"message": "head [M] body", "marker": "[M]",
                   "stop_marker": "[S]", "find": "rfind"})

# ── register_auto_load_scaffold (6) ──
add("register_auto_load_scaffold", {"scaffold": "[A]"}, scaffolds=[])
add("register_auto_load_scaffold", {"scaffold": ""}, scaffolds=["[A]", "[B]"])
add("register_auto_load_scaffold", {"scaffold": "[A]"}, scaffolds=["[A]", "[B]"])
add("register_auto_load_scaffold", {"scaffold": "[C]"}, scaffolds=["[A]", "[B]"])
add("register_auto_load_scaffold", {"scaffold": None}, scaffolds=["[A]"])
add("register_auto_load_scaffold", {"scaffold": "[NEW]"},
    scaffolds=[f"s{i}" for i in range(33)])

# ── _registered_auto_load_tail (7) ──
add("_registered_auto_load_tail", {"content": "[IMPORTANT: The \"work\" skill] typed text"},
    scaffolds=['[IMPORTANT: The "work" skill is', '[IMPORTANT: The "work" skill is auto-loaded'])
add("_registered_auto_load_tail", {"content": "no match here"}, scaffolds=["[A]", "[B]"])
add("_registered_auto_load_tail", {"content": "[EXACT]"}, scaffolds=["[EXACT]"])
add("_registered_auto_load_tail", {"content": "[A] tail", "x": 1}, scaffolds=["[A]"])
add("_registered_auto_load_tail", {"content": "anything"}, scaffolds=[])
add("_registered_auto_load_tail", {"content": "prefixed content"}, scaffolds=[""])
add("_registered_auto_load_tail", {"content": "abc"}, scaffolds=["a", "ab", "abc"])

# ── _auto_load_typed_text (7) ──
add("_auto_load_typed_text", {"content": al_header("work") + "  registered tail  "},
    scaffolds=[al_header("work")])
add("_auto_load_typed_text", {"content": "no footer and nothing registered"}, scaffolds=[])
add("_auto_load_typed_text", {"content": "pad " + DEND + "   final ask   "}, scaffolds=[])
add("_auto_load_typed_text",
    {"content": "x " + DEND + "\n[Skill setup note: hi]\n\nreal ask"}, scaffolds=[])
add("_auto_load_typed_text",
    {"content": "x " + DEND + "\n[Skill config (\nkey=1\n]\nconf ask"}, scaffolds=[])
add("_auto_load_typed_text",
    {"content": "x " + DEND + "\n[This skill has supporting files see /scripts/foo.js`).\nsup ask"},
    scaffolds=[])
add("_auto_load_typed_text",
    {"content": "x " + DEND + "  first " + DEND + "  last ask"}, scaffolds=[])

OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for case in CASES:
        f.write(json.dumps(case, ensure_ascii=False) + "\n")

counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print(f"wrote {len(CASES)} cases to {OUT}")
for f in sorted(counts):
    print(f"  {f}: {counts[f]}")
