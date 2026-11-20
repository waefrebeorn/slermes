#!/usr/bin/env python3
"""Generate tests/fixtures/cli_output_flow.jsonl — differential cases for the
UNCLAIMED hermes_cli/cli_output.py rendering-cluster FLOW ports
(src/cli/cli_output_flow.c).

Every case is {"func", "in"}; "in.seams" scripts the universal seam (single
value reused, array consumed per call with the last element repeating,
{"__raise__": kind, "msg": ...} scripts an exception, a MISSING script is
fail-closed RuntimeError on both sides). "in.state" injects the mutable
catalog/pack/overlay state for the stateful flows.

Domain (see the port header): ASCII inputs; paths are clean POSIX strings;
numbers in str()-computed positions are ints/strings (no integral floats);
set-valued seam scripts are arrays; parse_locale_file / layers.surface_catalog
scripts are objects or raises; load_locale_source non-mapping probes use
int/bool/null/list. >=5 cases per ported function.

Run:  python tests/gen_cli_output_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "cli_output_flow.jsonl")

HOME = "/home/u/.hermes"
LOCDIR = "/quarry/locales"


def case(func, seams=None, state=None, **inp):
    d = dict(inp)
    d["seams"] = dict(seams or {})
    if state is not None:
        d["state"] = state
    return {"func": func, "in": d}


def pack(lang, surface, messages, source="plugin", endonym=None, rtl=False, token=1):
    return {"lang": lang, "surface": surface, "messages": messages,
            "source": source, "endonym": endonym, "rtl": rtl, "token": token}


CASES = []

# ── print_truncated (cli_output.py) ──
PT = {"t": "5 more", "color": "COLORED", "print": None}
CASES += [
    case("print_truncated", PT, more=5, hint=""),
    case("print_truncated", PT, more=0, hint="use --limit 40 to see more"),
    case("print_truncated", {**PT, "t": "more not shown"}, more=None, hint=""),
    case("print_truncated", {**PT, "t": "more not shown"}, more=None, hint="see all"),
    case("print_truncated", PT, more=12, hint=""),
    case("print_truncated", {**PT, "t": {"__raise__": "ValueError", "msg": "boom"}}, more=5),
    case("print_truncated", {**PT, "color": {"__raise__": "KeyboardInterrupt", "msg": ""}}, more=5),
    case("print_truncated", {**PT, "print": {"__raise__": "RuntimeError", "msg": "io"}}, more=5),
    case("print_truncated", {}, more=5),                                   # t fail-closed
    case("print_truncated", {**PT, "t": None}, more=3),                    # str(None)
    case("print_truncated", {**PT, "color": None}, more=3),                # print(None)
    case("print_truncated", {**PT, "t": ["a", "b"]}, more=7),              # queue
]

# ── _write (secret_prompt.py) ──
WR = {"stdout.write": None, "stdout.flush": None}
CASES += [
    case("_write", WR, text="hello"),
    case("_write", WR, text=""),
    case("_write", {**WR, "stdout.write": {"__raise__": "ValueError", "msg": "w"}}, text="x"),
    case("_write", {**WR, "stdout.flush": {"__raise__": "KeyboardInterrupt", "msg": ""}}, text="x"),
    case("_write", {}, text="x"),                                          # write fail-closed
    case("_write", {"stdout.write": "discarded", "stdout.flush": None}, text="y"),
]

# ── _current_home (i18n.py) ──
CASES += [
    case("_current_home", {"get_hermes_home": HOME}),
    case("_current_home", {"get_hermes_home": None}),                      # str(None)
    case("_current_home", {"get_hermes_home": ""}),
    case("_current_home", {"get_hermes_home": {"__raise__": "RuntimeError", "msg": "h"}}),
    case("_current_home", {}),                                             # fail-closed
    case("_current_home", {"get_hermes_home": True}),                      # str(True)
]

# ── _config_language (i18n.py) ──
CL = {"get_hermes_home": HOME, "_config_language_cached": "de"}
CASES += [
    case("_config_language", CL),
    case("_config_language", {**CL, "_config_language_cached": None}),
    case("_config_language", {**CL, "_config_language_cached": ""}),
    case("_config_language", {**CL, "get_hermes_home": {"__raise__": "RuntimeError", "msg": "h"}}),
    case("_config_language", {**CL, "_config_language_cached": {"__raise__": "ValueError", "msg": "c"}}),
    case("_config_language", {}),                                          # fail-closed
]

# ── _resolve_language (i18n.py) ──
RL = {"get_secret": "de", "_normalize_lang": "de", "_config_language_cached": "fr",
      "environ.get": "ja"}
CASES += [
    case("_resolve_language", RL, home=HOME),
    case("_resolve_language", {**RL, "get_secret": ""}, home=HOME),        # falsy -> config
    case("_resolve_language", {**RL, "get_secret": None, "_config_language_cached": None}, home=HOME),  # -> en
    case("_resolve_language", {**RL, "get_secret": {"__raise__": "UnscopedSecretError", "msg": "u"}}, home=HOME),
    case("_resolve_language", {**RL, "get_secret": {"__raise__": "UnscopedSecretError", "msg": "u"},
                               "environ.get": None, "_config_language_cached": "es"}, home=HOME),
    case("_resolve_language", {**RL, "get_secret": {"__raise__": "RuntimeError", "msg": "r"}}, home=HOME),
    case("_resolve_language", {**RL, "get_secret": {"__raise__": "KeyboardInterrupt", "msg": ""}}, home=HOME),
    case("_resolve_language", {**RL, "get_secret": "", "_config_language_cached": {"__raise__": "ValueError", "msg": "c"}}, home=HOME),
    case("_resolve_language", {}, home=HOME),                              # fail-closed
    case("_resolve_language", {**RL, "get_secret": "EN", "_normalize_lang": "en"}, home=HOME),
    case("_resolve_language", {**RL, "get_secret": {"__raise__": "UnscopedSecretError", "msg": "u"},
                               "environ.get": {"__raise__": "RuntimeError", "msg": "e"}}, home=HOME),
]

# ── supported_languages (i18n.py) ──
SL = {"get_hermes_home": HOME, "layered_languages": ["pl", "de"]}
CASES += [
    case("supported_languages", SL, home=HOME),
    case("supported_languages", {**SL, "layered_languages": []}, home=HOME),
    case("supported_languages", {**SL, "layered_languages": ["pl"]}, home=None),
    case("supported_languages", SL, home=HOME, state={"supported_cache": {HOME: ["en", "xx"]}}),  # cache hit
    case("supported_languages", {**SL, "layered_languages": ["en", "zh"]}, home=HOME),
    case("supported_languages", {**SL, "layered_languages": {"__raise__": "ValueError", "msg": "l"}}, home=HOME),
    case("supported_languages", {"get_hermes_home": HOME}, home=HOME),     # layered fail-closed
    case("supported_languages", {**SL, "layered_languages": ["pl"]}, home=""),
    case("supported_languages", {**SL, "layered_languages": ["pl", "pl"]}, home=HOME),  # dedup
    case("supported_languages", {**SL, "layered_languages": ["sr-latn-rs"]}, home=HOME),
]

# ── resolve_language_id (i18n.py) ──
RI = {"get_hermes_home": HOME, "layered_languages": []}
CASES += [
    case("resolve_language_id", RI, value="en", home=HOME),
    case("resolve_language_id", {**RI, "layered_languages": ["pl"]}, value="PL", home=HOME),
    case("resolve_language_id", RI, value="chinese", home=HOME),           # alias zh
    case("resolve_language_id", RI, value="pt-br", home=HOME),             # alias pt
    case("resolve_language_id", RI, value="xx", home=HOME),                # -> None
    case("resolve_language_id", RI, value="zh-tw", home=HOME),             # alias zh-hant
    case("resolve_language_id", RI, value="", home=HOME),                  # empty -> None, no seams
    case("resolve_language_id", RI, value=5, home=HOME),                   # non-str -> None
    case("resolve_language_id", RI, value="en-us", home=HOME),             # alias en
    case("resolve_language_id", {**RI, "layered_languages": ["xx"]}, value="xx-yy", home=HOME),  # base xx
    case("resolve_language_id", {**RI, "layered_languages": {"__raise__": "ValueError", "msg": "l"}}, value="en", home=HOME),
    case("resolve_language_id", RI, value="en", home=HOME, state={"supported_cache": {HOME: ["en", "zz"]}}),
    case("resolve_language_id", RI, value="traditional-chinese", home=HOME),  # alias zh-hant
]

# ── _load_bundled (i18n.py) ──
LB = {"_locales_dir": LOCDIR, "path.is_file": True, "parse_locale_file": {"a": "b"}}
CASES += [
    case("_load_bundled", LB, lang="de"),
    case("_load_bundled", {**LB, "path.is_file": False}, lang="de"),       # {} no parse
    case("_load_bundled", {**LB, "parse_locale_file": {"__raise__": "ValueError", "msg": "p"}}, lang="de"),
    case("_load_bundled", {**LB, "parse_locale_file": {"__raise__": "KeyboardInterrupt", "msg": ""}}, lang="de"),
    case("_load_bundled", {**LB, "parse_locale_file": {"__raise__": "RuntimeError", "msg": "p"}}, lang="de"),
    case("_load_bundled", {**LB, "_locales_dir": {"__raise__": "RuntimeError", "msg": "d"}}, lang="de"),
    case("_load_bundled", {}, lang="de"),                                  # fail-closed
    case("_load_bundled", {**LB, "path.is_file": {"__raise__": "OSError", "msg": "f"}}, lang="de"),
    case("_load_bundled", LB, lang="zh-hant"),
]

# ── surface_catalog (i18n.py) ──
SC = {"_normalize_lang": "de", "get_hermes_home": HOME,
      "layers.surface_catalog": {"x": "y"},
      "_locales_dir": LOCDIR, "path.is_file": True, "parse_locale_file": {"a": "1", "b": "2"}}
CASES += [
    case("surface_catalog", SC, lang="de", surface="core"),
    case("surface_catalog", {**SC, "layers.surface_catalog": {"b": "3", "c": "4"}}, lang="de", surface="tui"),
    case("surface_catalog", {**SC, "path.is_file": False}, lang="de", surface="tui"),
    case("surface_catalog", {**SC, "_normalize_lang": {"__raise__": "ValueError", "msg": "n"}}, lang="de"),
    case("surface_catalog", {**SC, "layers.surface_catalog": {"__raise__": "ValueError", "msg": "s"}}, lang="de"),
    case("surface_catalog", {**SC, "parse_locale_file": {"__raise__": "RuntimeError", "msg": "p"}}, lang="de", surface="tui"),
    case("surface_catalog", {**SC, "get_hermes_home": {"__raise__": "RuntimeError", "msg": "h"}}, lang="de"),
    case("surface_catalog", {}, lang="de"),                                # fail-closed
    case("surface_catalog", SC, lang="DE", surface="desktop"),
]

# ── language_surfaces (i18n.py) ──
LS = {"get_hermes_home": HOME,
      "overlay_surfaces": [["pl", "core"]], "pack_surfaces": [["de", "tui"]],
      "overlay_layer": {"a": "b"}, "pack_layer": {"c": "d"},
      "_locales_dir": LOCDIR, "_bundled_surfaces": [["en", "core"]]}
CASES += [
    case("language_surfaces", LS, home=HOME),
    case("language_surfaces", {**LS, "overlay_layer": {}, "pack_layer": {}}, home=HOME),  # exclude
    case("language_surfaces", {**LS, "overlay_layer": {"a": "b"}}, home=HOME),            # short-circuit
    case("language_surfaces", {**LS, "overlay_surfaces": [], "pack_surfaces": [],
                               "_bundled_surfaces": [["en", "core"], ["en", "tui"]]}, home=HOME),
    case("language_surfaces", {**LS, "overlay_surfaces": [], "pack_surfaces": [],
                               "_bundled_surfaces": [["en", "tui"], ["en", "core"]]}, home=HOME),  # SURFACES order
    case("language_surfaces", {**LS, "overlay_surfaces": {"__raise__": "ValueError", "msg": "o"}}, home=HOME),
    case("language_surfaces", {**LS, "overlay_layer": {"__raise__": "KeyboardInterrupt", "msg": ""}}, home=HOME),
    case("language_surfaces", {"get_hermes_home": HOME}, home=HOME),       # overlay_surfaces fail-closed
    case("language_surfaces", {**LS, "overlay_surfaces": [["pl", "core"]]}, home=None),
    case("language_surfaces", {**LS, "overlay_surfaces": [], "pack_surfaces": [],
                               "_bundled_surfaces": [["pl", "xxx"]]}, home=HOME),         # unknown surface filtered
    case("language_surfaces", {**LS, "pack_layer": {"__raise__": "RuntimeError", "msg": "p"},
                               "overlay_layer": {}}, home=HOME),
]

# ── normalize_language_id (i18n_layers.py) ──
CASES += [
    case("normalize_language_id", value="EN"),
    case("normalize_language_id", value=" pt_BR "),
    case("normalize_language_id", value=""),
    case("normalize_language_id", value=5),
    case("normalize_language_id", value=None),
    case("normalize_language_id", value="Zh-Hant"),
    case("normalize_language_id", value=True),
    case("normalize_language_id", value="   "),
]

# ── is_language_id (i18n_layers.py) ──
CASES += [
    case("is_language_id", value="en"),
    case("is_language_id", value="e"),
    case("is_language_id", value="pt-br"),
    case("is_language_id", value="pt_br"),
    case("is_language_id", value="en-x"),
    case("is_language_id", value="en-abcdefgh"),
    case("is_language_id", value="en-abcdefghi"),
    case("is_language_id", value=""),
    case("is_language_id", value=5),
    case("is_language_id", value="abcd"),
    case("is_language_id", value="ENG"),
]

# ── flatten (i18n_layers.py) ──
CASES += [
    case("flatten", node={"a": {"b": "c"}}, prefix=""),
    case("flatten", node={"a": "x", "b": 5}, prefix=""),
    case("flatten", node={"a": [1, 2]}, prefix="p"),
    case("flatten", node="leaf", prefix="p"),
    case("flatten", node={"a": {"b": {"c": "d"}}}, prefix="", out={"pre": "x"}),
    case("flatten", node={"a": None, "b": True}, prefix=""),
    case("flatten", node={}, prefix=""),
    case("flatten", node={"a": "x"}, prefix="p"),
    case("flatten", node={"a": {"b": "c"}, "d": "e"}, prefix="", out={"z": "0"}),
]

# ── non_text_leaves (i18n_layers.py) ──
CASES += [
    case("non_text_leaves", node={"a": 5, "b": "x"}, prefix=""),
    case("non_text_leaves", node={"a": {"b": [1]}}, prefix=""),
    case("non_text_leaves", node={"a": "x"}, prefix=""),
    case("non_text_leaves", node=5, prefix=""),
    case("non_text_leaves", node=5, prefix="p"),
    case("non_text_leaves", node={"a": None, "b": True, "c": {"d": 1.5}}, prefix=""),
    case("non_text_leaves", node=[1, 2], prefix=""),
    case("non_text_leaves", node={}, prefix=""),
]

# ── overlay_dir (i18n_layers.py) ──
CASES += [
    case("overlay_dir", home="/h"),
    case("overlay_dir", home=""),
    case("overlay_dir", home="/h/"),
    case("overlay_dir", home="/"),
    case("overlay_dir", home="/a/b"),
]

# ── load_locale_source (i18n_layers.py) ──
CASES += [
    case("load_locale_source", source={"a": {"b": "c"}}),
    case("load_locale_source", {"parse_locale_file": {"k": "v"}}, source="/p/x.yaml"),
    case("load_locale_source", source=5),
    case("load_locale_source", source=None),
    case("load_locale_source", source=True),
    case("load_locale_source", source=[1]),
    case("load_locale_source", {"parse_locale_file": {"__raise__": "ValueError", "msg": "p"}}, source="x.yaml"),
    case("load_locale_source", {}, source="x.yaml"),                       # fail-closed
]

# ── pack_layer (i18n_layers.py) ──
P1 = pack("pl", "core", {"a": "1"}, source="p1", token=1)
P2 = pack("pl", "core", {"a": "2", "b": "3"}, source="p2", token=2)
P3 = pack("pl", "tui", {"t": "9"}, source="p3", token=3)
CASES += [
    case("pack_layer", state={"packs": [P1]}, lang="pl", surface="core"),
    case("pack_layer", state={"packs": [P1, P2]}, lang="pl", surface="core"),   # last wins
    case("pack_layer", state={"packs": [P1], "pack_cache": {"pl\x1fcore": {"cached": "yes"}}}, lang="pl", surface="core"),
    case("pack_layer", state={"packs": [P3]}, lang="pl", surface="core"),       # no match -> {}
    case("pack_layer", state={"packs": [P1, P3]}, lang="pl", surface="tui"),
    case("pack_layer", state={"packs": []}, lang="pl", surface="core"),
    case("pack_layer", state={"packs": [P1], "pack_cache": {"pl\x1fcore": None}}, lang="pl", surface="core"),  # null recompute
]

# ── pack_info (i18n_layers.py) ──
CASES += [
    case("pack_info", state={"packs": [pack("pl", "core", {}, endonym="Polski", rtl=False)]}, lang="pl"),
    case("pack_info", state={"packs": []}, lang="pl"),                          # None
    case("pack_info", state={"packs": [pack("pl", "core", {}, endonym="Old", rtl=False, token=1),
                                       pack("pl", "tui", {}, endonym="Nowy", rtl=True, token=2)]}, lang="pl"),
    case("pack_info", state={"packs": [pack("pl", "core", {}, endonym="Polski", rtl=False, token=1),
                                       pack("pl", "tui", {}, endonym="", rtl=True, token=2)]}, lang="pl"),  # "" endonym rtl
    case("pack_info", state={"packs": [pack("pl", "core", {}, endonym=None, rtl=True, token=1),
                                       pack("pl", "tui", {}, endonym=None, rtl=False, token=2)]}, lang="pl"),
    case("pack_info", state={"packs": [pack("de", "core", {}, endonym="Deutsch")]}, lang="pl"),  # other lang
]

# ── overlay_layer (i18n_layers.py) ──
OL = {"path.is_file": True, "parse_locale_file": {"a": "b"}}
CASES += [
    case("overlay_layer", OL, home="/h", lang="pl", surface="core"),
    case("overlay_layer", {**OL, "path.is_file": False}, home="/h", lang="pl", surface="core"),
    case("overlay_layer", {**OL, "parse_locale_file": {"__raise__": "ValueError", "msg": "p"}}, home="/h", lang="pl"),
    case("overlay_layer", {**OL, "parse_locale_file": {"__raise__": "KeyboardInterrupt", "msg": ""}}, home="/h", lang="pl"),
    case("overlay_layer", OL, home="/h", lang="pl", surface="core",
         state={"overlay_cache": {"/h\x1fpl\x1fcore": {"cached": "yes"}}}),    # cache hit
    case("overlay_layer", OL, home="/h", lang="pl", surface="tui"),
    case("overlay_layer", {}, home="/h", lang="pl"),                          # is_file fail-closed
    case("overlay_layer", {**OL, "parse_locale_file": None}, home="/h", lang="pl"),  # flat None cached
]

# ── scan_locale_dir (i18n_layers.py) ──
SD = {"path.is_dir": True,
      "path.iterdir": [LOCDIR + "/pl.yaml", LOCDIR + "/en.tui.yaml"],
      "path.is_file": True}
CASES += [
    case("scan_locale_dir", SD, directory=LOCDIR),
    case("scan_locale_dir", {**SD, "path.is_dir": False}, directory=LOCDIR),
    case("scan_locale_dir", {**SD, "path.iterdir": [LOCDIR + "/README.md", LOCDIR + "/pl.yaml"]}, directory=LOCDIR),
    case("scan_locale_dir", {**SD, "path.iterdir": [LOCDIR + "/pl.tiu.yaml"]}, directory=LOCDIR),  # unknown surface
    case("scan_locale_dir", {**SD, "path.iterdir": [LOCDIR + "/pl.yaml"], "path.is_file": False}, directory=LOCDIR),
    case("scan_locale_dir", {**SD, "path.iterdir": [LOCDIR + "/en.tui.yaml", LOCDIR + "/pl.yaml"]}, directory=LOCDIR),  # sort
    case("scan_locale_dir", {**SD, "path.is_dir": {"__raise__": "OSError", "msg": "d"}}, directory=LOCDIR),
    case("scan_locale_dir", {}, directory=LOCDIR),                            # fail-closed
    case("scan_locale_dir", {**SD, "path.iterdir": {"__raise__": "RuntimeError", "msg": "i"}}, directory=LOCDIR),
    case("scan_locale_dir", {**SD, "path.iterdir": [LOCDIR + "/pl.yml", LOCDIR + "/zh-hant.yaml"]}, directory=LOCDIR),
    case("scan_locale_dir", {**SD, "path.iterdir": [LOCDIR + "/pl.desktop.yaml", LOCDIR + "/abc-tui.yaml"]}, directory=LOCDIR),
]


def main():
    # List-valued seam returns must be wrapped as a single-element queue: under
    # the harness/oracle queue semantics a BARE array is consumed one element
    # per call (and [] is fail-closed), so a list RETURN scripts as [value].
    list_seams = {"layered_languages", "overlay_surfaces", "pack_surfaces",
                  "_bundled_surfaces", "path.iterdir"}
    for c in CASES:
        for k in list_seams:
            v = c["in"]["seams"].get(k)
            if isinstance(v, list):
                c["in"]["seams"][k] = [v]
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for fn in sorted(counts):
        flag = "" if counts[fn] >= 5 else "  (<5!)"
        print("  %-24s %d%s" % (fn, counts[fn], flag))
    assert all(v >= 5 for v in counts.values()), "every function needs >=5 cases"
    assert len(counts) == 20, "expected 20 ported functions"


if __name__ == "__main__":
    main()
