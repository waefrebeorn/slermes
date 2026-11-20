#!/usr/bin/env python3
"""Generate tests/fixtures/i18n_flow2.jsonl — differential cases for the
agent/i18n.py FLOW remainder ports (src/agent/i18n_flow2.c) plus the composed
agent/i18n_layers.py clear_cache.

Every case is {"func", "in"}; "in.seams" scripts the universal seam (single
value reused, array consumed per call with the last element repeating,
{"__raise__": kind, "msg": ...} scripts an exception, a MISSING script is
fail-closed RuntimeError on both sides). "in.state" injects the mutable
catalog/lru/layer state for the stateful flows.

Domain (see the port header): ASCII inputs; paths are clean POSIX strings;
numbers in str()-computed positions are ints/strings (no integral floats);
scan_locale_dir scripts are arrays of exactly-3 string triples (wrapped as a
single-element queue); _load_bundled / overlay_layer / pack_layer scripts are
objects, null or raises; _current_home / _resolve_language scripts are strings;
resolve_language_id scripts are strings or null; bundled_surfaces_cache values
are SORTED [lang, surface] pair arrays; lru stores stay below maxsize (<=3
keys). t's format templates use {name} fields with optional !s/!r/!a and EMPTY
format specs, plus the malformed-template ValueError family. >=5 cases per
ported function.

Run:  python tests/gen_i18n_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "i18n_flow2.jsonl")

HOME = "/home/u/.hermes"
LOCDIR = "/quarry/locales"


def case(func, seams=None, state=None, **inp):
    d = dict(inp)
    d["seams"] = dict(seams or {})
    if state is not None:
        d["state"] = state
    return {"func": func, "in": d}


def ck(home, lang):
    return home + "\x1f" + lang


CASES = []

# ── _locales_dir (i18n.py) ──
LD = {"os.getenv": LOCDIR, "path.is_dir": True}
CASES += [
    case("_locales_dir", LD),                                            # sealed override dir
    case("_locales_dir", {**LD, "path.is_dir": False, "project_root_str": "/repo"}),
    case("_locales_dir", {"os.getenv": "", "project_root_str": "/repo"}),
    case("_locales_dir", {"os.getenv": "   ", "project_root_str": "/repo"}),  # strip -> empty
    case("_locales_dir", {"os.getenv": "  " + LOCDIR + "  ", "path.is_dir": True}),
    case("_locales_dir", {"os.getenv": None}),                           # None.strip AttributeError
    case("_locales_dir", {"os.getenv": 5}),                              # int.strip AttributeError
    case("_locales_dir", {"os.getenv": "", "project_root_str": None}),   # Path(None) TypeError
    case("_locales_dir", {"os.getenv": "", "project_root_str": ""}),     # Path("")/"locales"
    case("_locales_dir", {"os.getenv": "", "project_root_str": "/"}),    # root join
    case("_locales_dir", {}),                                            # os.getenv fail-closed
    case("_locales_dir", {"os.getenv": {"__raise__": "KeyboardInterrupt", "msg": ""}}),
    case("_locales_dir", {**LD, "path.is_dir": {"__raise__": "ValueError", "msg": "d"}}),
    case("_locales_dir", {"os.getenv": "", "project_root_str": {"__raise__": "RuntimeError", "msg": "p"}}),
    case("_locales_dir", {**LD, "path.is_dir": "yes"}),                  # truthy non-bool
    case("_locales_dir", {"os.getenv": LOCDIR, "path.is_dir": False, "project_root_str": "/repo"}),
]

# ── _bundled_surfaces (i18n.py) ──
BS = {"scan_locale_dir": [["pl", "core", LOCDIR + "/pl.yaml"],
                          ["de", "tui", LOCDIR + "/de.tui.yaml"],
                          ["pl", "core", LOCDIR + "/pl2.yaml"]]}
CASES += [
    case("_bundled_surfaces", BS, locales_dir=LOCDIR),                   # dedup + sort
    case("_bundled_surfaces", {"scan_locale_dir": [[]]}, locales_dir=LOCDIR),  # empty scan
    case("_bundled_surfaces", {}, locales_dir=LOCDIR,
         state={"bundled_surfaces_cache": {LOCDIR: [["de", "core"]]}}),  # lru hit, no seam
    case("_bundled_surfaces", {"scan_locale_dir": [["pl", "core", "/l/pl.yaml"]]}, locales_dir=""),
    case("_bundled_surfaces", {"scan_locale_dir": None}, locales_dir=LOCDIR),  # TypeError
    case("_bundled_surfaces", {"scan_locale_dir": {"__raise__": "ValueError", "msg": "s"}}, locales_dir=LOCDIR),
    case("_bundled_surfaces", {}, locales_dir=LOCDIR),                   # fail-closed
    case("_bundled_surfaces", {"scan_locale_dir": {"__raise__": "KeyboardInterrupt", "msg": ""}}, locales_dir=LOCDIR),
    case("_bundled_surfaces", {"scan_locale_dir": [["pl", "core"]]}, locales_dir=LOCDIR),  # unpack 2
    case("_bundled_surfaces", {"scan_locale_dir": [["pl", "core", "p", "x"]]}, locales_dir=LOCDIR),  # unpack 4
    case("_bundled_surfaces", {"scan_locale_dir": [5]}, locales_dir=LOCDIR),   # non-triple element
    case("_bundled_surfaces", {"scan_locale_dir": [["pl", "tiu", LOCDIR + "/pl.tiu.yaml"]]}, locales_dir=LOCDIR),
    case("_bundled_surfaces", {"scan_locale_dir": [["zh-hant", "desktop", "/l/zh-hant.desktop.yaml"],
                                                   ["ar", "core", "/l/ar.yaml"]]}, locales_dir=LOCDIR),
]

# ── _normalize_lang (i18n.py) ──
NL = {"resolve_language_id": "de"}
CASES += [
    case("_normalize_lang", NL, value="de", home=HOME),
    case("_normalize_lang", {"resolve_language_id": None}, value="xx", home=HOME),  # -> en
    case("_normalize_lang", {"resolve_language_id": ""}, value="xx", home=HOME),    # falsy -> en
    case("_normalize_lang", {"resolve_language_id": "en"}, value=None, home=None),  # args stripped
    case("_normalize_lang", {"resolve_language_id": "ja"}, value=5, home=HOME),     # non-str value
    case("_normalize_lang", {"resolve_language_id": {"__raise__": "ValueError", "msg": "r"}}, value="de", home=HOME),
    case("_normalize_lang", {"resolve_language_id": {"__raise__": "KeyboardInterrupt", "msg": ""}}, value="de", home=HOME),
    case("_normalize_lang", {}, value="de", home=HOME),                  # fail-closed
    case("_normalize_lang", {"resolve_language_id": "zh-hant"}, value="zh-tw", home=HOME),
    case("_normalize_lang", {"resolve_language_id": "en"}, home=HOME),   # value absent -> None
    case("_normalize_lang", {"resolve_language_id": {"__raise__": "MemoryError", "msg": "m"}}, value="de", home=HOME),
    # Unicode-trio str() is attribute-derived (always "" under the make_exc
    # bypass construction), so only an empty msg round-trips the raise protocol
    case("_normalize_lang", {"resolve_language_id": {"__raise__": "UnicodeDecodeError", "msg": ""}}, value="de", home=HOME),
]

# ── _flatten_into (i18n.py) ──
FL = {"flatten": {"a.b": "x"}}
CASES += [
    case("_flatten_into", FL, node={"a": {"b": "x"}}, prefix="", out={}),
    case("_flatten_into", {"flatten": {}}, node=None, prefix="", out={}),
    case("_flatten_into", {"flatten": {"k": "v"}}, node={"k": "v"}, prefix="pfx", out=None),
    case("_flatten_into", {"flatten": {"p.a": "1"}}, node={"a": "1"}, prefix="p", out={"old": "keep"}),
    case("_flatten_into", {"flatten": {"__raise__": "ValueError", "msg": "f"}}, node={}, prefix="", out={}),
    case("_flatten_into", {"flatten": {"__raise__": "SystemExit", "msg": "0"}}, node={}, prefix="", out={}),
    case("_flatten_into", {}, node={"a": "b"}, prefix="", out={}),       # fail-closed
    case("_flatten_into", {"flatten": {"a": "1", "b.c": "2"}}, node={"a": "1", "b": {"c": "2"}}, prefix="", out=None),
]

# ── _load_catalog (i18n.py) ──
LC = {"_load_bundled": {"a": "1"}, "overlay_layer": {"a": "2", "b": "3"}, "pack_layer": {"b": "4"}}
CASES += [
    case("_load_catalog", LC, lang="de", home=HOME),                     # packs > overlay > bundled
    case("_load_catalog", {**LC, "_current_home": HOME}, lang="de", home=None),
    case("_load_catalog", {**LC, "_current_home": HOME}, lang="de", home=""),
    case("_load_catalog", {}, lang="de", home=HOME,
         state={"catalog_cache": {ck(HOME, "de"): {"k": "v"}}}),         # cache hit, no seams
    case("_load_catalog", LC, lang="de", home=HOME,
         state={"catalog_cache": {ck(HOME, "de"): None}}),               # present-null recomputes
    case("_load_catalog", {**LC, "_load_bundled": None}, lang="de", home=HOME),  # None.update
    case("_load_catalog", {**LC, "overlay_layer": None}, lang="de", home=HOME),  # update(None)
    case("_load_catalog", {**LC, "pack_layer": None}, lang="de", home=HOME),
    case("_load_catalog", {**LC, "_load_bundled": {"__raise__": "ValueError", "msg": "b"}}, lang="de", home=HOME),
    case("_load_catalog", {**LC, "overlay_layer": {"__raise__": "KeyboardInterrupt", "msg": ""}}, lang="de", home=HOME),
    case("_load_catalog", {**LC, "pack_layer": {"__raise__": "RuntimeError", "msg": "p"}}, lang="de", home=HOME),
    case("_load_catalog", {}, lang="de", home=HOME),                     # _load_bundled fail-closed
    case("_load_catalog", {"_current_home": {"__raise__": "RuntimeError", "msg": "h"}}, lang="de", home=None),
    case("_load_catalog", {**LC, "overlay_layer": [["a", "b"]]}, lang="de", home=HOME),  # update(list)
    case("_load_catalog", {"_load_bundled": {"z": "1", "a": "2"}, "overlay_layer": {"a": "3"}, "pack_layer": {}},
         lang="de", home=HOME),                                          # first-position key order
    case("_load_catalog", {"_load_bundled": {}, "overlay_layer": {}, "pack_layer": {}}, lang="en", home=HOME),
    case("_load_catalog", {"_load_bundled": {"e": "1"}, "overlay_layer": {}, "pack_layer": {}}, lang="", home=HOME),
]

# ── _config_language_cached (i18n.py) ──
CC = {"load_config_readonly": {"display": {"language": "de"}}, "resolve_language_id": "de"}
CASES += [
    case("_config_language_cached", CC, hermes_home=HOME),
    case("_config_language_cached", {}, hermes_home=HOME,
         state={"config_lang_cache": {HOME: "fr"}}),                     # lru hit
    case("_config_language_cached", {}, hermes_home=HOME,
         state={"config_lang_cache": {HOME: None}}),                     # cached None HITS
    case("_config_language_cached", {"load_config_readonly": {}}, hermes_home=HOME),
    case("_config_language_cached", {"load_config_readonly": {"display": None}}, hermes_home=HOME),
    case("_config_language_cached", {"load_config_readonly": {"display": {}}}, hermes_home=HOME),
    case("_config_language_cached", {"load_config_readonly": {"display": {"language": ""}}}, hermes_home=HOME),
    case("_config_language_cached", {**CC, "load_config_readonly": {"display": {"language": "xx"}},
                                     "resolve_language_id": None}, hermes_home=HOME),  # -> en
    case("_config_language_cached", {"load_config_readonly": {"__raise__": "ValueError", "msg": "c"}}, hermes_home=HOME),
    case("_config_language_cached", {"load_config_readonly": {"__raise__": "KeyboardInterrupt", "msg": ""}}, hermes_home=HOME),
    case("_config_language_cached", {"load_config_readonly": None}, hermes_home=HOME),  # None.get caught
    case("_config_language_cached", {"load_config_readonly": {"display": "de"}}, hermes_home=HOME),  # str.get caught
    case("_config_language_cached", {"load_config_readonly": {"display": {"language": 0}}}, hermes_home=HOME),
    case("_config_language_cached", {**CC, "load_config_readonly": {"display": {"language": 5}},
                                     "resolve_language_id": "ja"}, hermes_home=HOME),
    case("_config_language_cached", {**CC, "resolve_language_id": {"__raise__": "RuntimeError", "msg": "r"}}, hermes_home=HOME),
    case("_config_language_cached", {**CC, "resolve_language_id": {"__raise__": "GeneratorExit", "msg": ""}}, hermes_home=HOME),
    case("_config_language_cached", {}, hermes_home=HOME),               # fail-closed caught -> None
    case("_config_language_cached", {"load_config_readonly": {"display": 0}}, hermes_home=HOME),
    case("_config_language_cached", {"load_config_readonly": {"display": []}}, hermes_home=HOME),
    case("_config_language_cached", {"load_config_readonly": {"display": {"language": "de"}, "other": 1}}, hermes_home=HOME),
    case("_config_language_cached", {"load_config_readonly": {"__raise__": "SystemExit", "msg": "0"}}, hermes_home=HOME),
    case("_config_language_cached", {"load_config_readonly": {"__raise__": "RecursionError", "msg": "r"}}, hermes_home=HOME),
    case("_config_language_cached", {"load_config_readonly": {"__raise__": "UserWarning", "msg": "w"}}, hermes_home=HOME),
    case("_config_language_cached", {"load_config_readonly": {"__raise__": "MyCustomError", "msg": "c"}}, hermes_home=HOME),
    case("_config_language_cached", {**CC, "resolve_language_id": {"__raise__": "KeyboardInterrupt", "msg": ""}}, hermes_home=HOME),
    case("_config_language_cached", CC, hermes_home=""),                 # empty-home lru key
]

# ── reset_language_cache (i18n.py) ──
FULL_STATE = {
    "catalog_cache": {ck(HOME, "de"): {"a": "1"}},
    "supported_cache": {HOME: ["en", "de"]},
    "config_lang_cache": {HOME: "de"},
    "bundled_surfaces_cache": {LOCDIR: [["de", "core"], ["pl", "tui"]]},
    "pack_cache": {ck("pl", "core"): {"p": "1"}},
    "overlay_cache": {ck(HOME, "pl") + "\x1fcore": {"o": "1"}},
    "overlay_files_cache": {HOME: [["pl", "core"]]},
}
CASES += [
    case("reset_language_cache", {}, state={}),
    case("reset_language_cache", {}, state=FULL_STATE),
    case("reset_language_cache", {}, state={"catalog_cache": {ck(HOME, "de"): {"a": "1"}},
                                            "config_lang_cache": {HOME: "de"}}),
    case("reset_language_cache", {}, state={"pack_cache": {ck("pl", "core"): {"p": "1"}},
                                            "overlay_cache": {ck(HOME, "pl") + "\x1fcore": {"o": "1"}},
                                            "overlay_files_cache": {HOME: [["pl", "core"]]}}),
    case("reset_language_cache", {}, state={"supported_cache": {HOME: ["en"]},
                                            "bundled_surfaces_cache": {LOCDIR: [["de", "core"]]}}),
    case("reset_language_cache", {}, state={"catalog_cache": {ck(HOME, "en"): None}}),
]

# ── get_language (i18n.py) ──
GL = {"_current_home": HOME, "_resolve_language": "de"}
CASES += [
    case("get_language", GL),
    case("get_language", {**GL, "_current_home": None, "_resolve_language": "en"}),  # args stripped
    case("get_language", {**GL, "_current_home": "", "_resolve_language": "fr"}),
    case("get_language", {**GL, "_current_home": {"__raise__": "ValueError", "msg": "h"}}),
    case("get_language", {**GL, "_resolve_language": {"__raise__": "KeyboardInterrupt", "msg": ""}}),
    case("get_language", {}),                                            # fail-closed
    case("get_language", {**GL, "_resolve_language": None}),
]

# ── t (i18n.py) ──
def tseams(catalog_de, catalog_en=None, resolve="de", **extra):
    s = {"_current_home": HOME, "resolve_language_id": resolve,
         "_load_bundled": catalog_de if catalog_en is None else [catalog_de, catalog_en],
         "overlay_layer": {}, "pack_layer": {}}
    s.update(extra)
    return s


CASES += [
    case("t", tseams({"a.b": "Hallo {name}"}), key="a.b", lang="de", kwargs={"name": "Welt"}),
    case("t", {**tseams({"k": "v"}), "_resolve_language": "de"}, key="k", lang=None),
    case("t", {**tseams({"k": "v"}), "_resolve_language": "de"}, key="k", lang=""),
    case("t", tseams({}, {"k": "English"}), key="k", lang="de"),         # en fallback
    case("t", tseams({}, {}), key="missing.key", lang="de"),             # bare key
    case("t", tseams({}), key="missing.key", lang="en"),                 # en miss, no 2nd load
    case("t", tseams({"k": "plain"}), key="k", lang="de"),               # no kwargs
    case("t", tseams({"k": "plain"}), key="k", lang="de", kwargs={}),    # empty kwargs
    case("t", tseams({"k": "{missing}"}), key="k", lang="de", kwargs={"name": "x"}),   # KeyError
    case("t", tseams({"k": "{0}"}), key="k", lang="de", kwargs={"name": "x"}),         # IndexError
    case("t", tseams({"k": "{}"}), key="k", lang="de", kwargs={"name": "x"}),          # auto IndexError
    case("t", tseams({"k": "}"}), key="k", lang="de", kwargs={"name": "x"}),           # ValueError
    case("t", tseams({"k": "{a"}), key="k", lang="de", kwargs={"name": "x"}),
    case("t", tseams({"k": "{a!x}"}), key="k", lang="de", kwargs={"a": "x"}),
    case("t", tseams({"k": "{name!r}"}), key="k", lang="de", kwargs={"name": "bob"}),
    case("t", tseams({"k": "{n!s}"}), key="k", lang="de", kwargs={"n": 5}),
    case("t", tseams({"k": "{name!a}"}), key="k", lang="de", kwargs={"name": "bob"}),
    case("t", tseams({"k": "{{literal}}"}), key="k", lang="de", kwargs={"name": "x"}),
    case("t", tseams({"k": "{name!r}"}), key="k", lang="de", kwargs={"name": "it's"}),
    case("t", tseams({"k": "{n}"}), key="k", lang="de", kwargs={"n": 7}),
    case("t", tseams({"k": "{b}"}), key="k", lang="de", kwargs={"b": True}),
    case("t", tseams({"k": "{z}"}), key="k", lang="de", kwargs={"z": None}),
    case("t", {}, key="k", lang="de", state={"catalog_cache": {ck(HOME, "de"): {"k": 7}}}),
    case("t", {}, key="k", lang="de", kwargs={"a": 1},
         state={"catalog_cache": {ck(HOME, "de"): {"k": 7}}}),           # int.format AttributeError
    case("t", {**tseams({"k": "v"}), "_current_home": {"__raise__": "RuntimeError", "msg": "h"}}, key="k", lang="de"),
    case("t", {**tseams({"k": "v"}), "resolve_language_id": {"__raise__": "ValueError", "msg": "r"}}, key="k", lang="de"),
    case("t", {**tseams({"k": "v"}), "_load_bundled": {"__raise__": "RuntimeError", "msg": "b"}}, key="k", lang="de"),
    case("t", {}, key="k", lang="de"),                                   # fail-closed
    case("t", {"_current_home": HOME, "resolve_language_id": "de"}, key="k", lang="de",
         state={"catalog_cache": {ck(HOME, "de"): {"k": "cached"}}}),    # cache hit
    case("t", tseams({"k": "v"}, resolve="ja"), key="k", lang=5),        # truthy non-str lang
    case("t", tseams({"k": "{name!r:}"}), key="k", lang="de", kwargs={"name": "bob"}),
    case("t", tseams({"k": "{missing!x}"}), key="k", lang="de", kwargs={"a": "x"}),   # KeyError first
    case("t", tseams({"k": "{a!}"}), key="k", lang="de", kwargs={"a": "x"}),
    case("t", tseams({"k": "{a!sx}"}), key="k", lang="de", kwargs={"a": "x"}),
    case("t", tseams({"k": "{a:>5"}), key="k", lang="de", kwargs={"a": "x"}),
    case("t", tseams({"k": "{a{b}"}), key="k", lang="de", kwargs={"a": "x"}),
    case("t", tseams({"k": "{a!"}), key="k", lang="de", kwargs={"a": "x"}),
    case("t", tseams({"k": None}, {"k": "from-en"}), key="k", lang="de"),  # present-null -> fallback
    case("t", tseams({"k": "hi {name}, bye {name!r}"}), key="k", lang="de", kwargs={"name": "bob"}),
    case("t", tseams({"k": "{01}"}), key="k", lang="de", kwargs={"a": "x"}),
    case("t", tseams({"k": "v"}), key="k", lang="de",
         state={"catalog_cache": {ck(HOME, "de"): {"k": "old"}, ck(HOME, "en"): {"k": "en-old"}}}),
    # queue LAST-element repeat: 1-element _load_bundled queue consumed by de + en loads
    case("t", {"_current_home": HOME, "resolve_language_id": "de",
               "_load_bundled": [{"k": "shared"}], "overlay_layer": {}, "pack_layer": {}},
         key="k", lang="de"),
    case("t", tseams({}, resolve=None), key="k", lang="xx"),             # normalize -> en, no fallback
    case("t", tseams({"k": "{name!r}"}), key="k", lang="de", kwargs={"name": "a'b\"c"}),
    case("t", tseams({"k": "{name!r}"}), key="k", lang="de", kwargs={"name": "a\x01b"}),
    case("t", tseams({"k": "{a-b}"}), key="k", lang="de", kwargs={"a-b": "hyphen"}),
    case("t", tseams({"k": "{ 1 }"}), key="k", lang="de", kwargs={" 1 ": "space"}),
    case("t", tseams({"k": "{a::}"}), key="k", lang="de", kwargs={"a": "x"}),  # bad spec -> ValueError fallback
    case("t", tseams({"k": "{name!s} and {n!s}"}), key="k", lang="de", kwargs={"name": "bob", "n": 3}),
    case("t", tseams({}), key="{name}", lang="de", kwargs={"name": "x"}),  # bare KEY gets formatted
    case("t", tseams({}), key="}", lang="de", kwargs={"name": "x"}),       # malformed bare key -> fallback
    case("t", {**tseams({"k": "v"}), "_current_home": ""}, key="k", lang="de"),  # home "" -> 2nd _current_home
]

# ── clear_cache (i18n_layers.py) ──
CASES += [
    case("clear_cache", {}, state={}),
    case("clear_cache", {}, state={"pack_cache": {ck("pl", "core"): {"p": "1"}},
                                   "overlay_cache": {ck(HOME, "pl") + "\x1fcore": {"o": "1"}},
                                   "overlay_files_cache": {HOME: [["pl", "core"]]}}),
    case("clear_cache", {}, state={"pack_cache": {ck("pl", "core"): {"p": "1"}}}),
    case("clear_cache", {}, state={"overlay_files_cache": {HOME: [["de", "tui"], ["pl", "core"]]}}),
    case("clear_cache", {}, state={"pack_cache": {ck("pl", "core"): {"p": "1"}, ck("de", "tui"): {"q": "2"}},
                                   "overlay_cache": {ck(HOME, "pl") + "\x1fcore": {"o": "1"}},
                                   "overlay_files_cache": {HOME: [["pl", "core"]], "/other": [["de", "core"]]}}),
]


def main():
    # List-valued seam returns must be wrapped as a single-element queue: under
    # the harness/oracle queue semantics a BARE array is consumed one element
    # per call (and [] is fail-closed), so a list RETURN scripts as [value].
    list_seams = {"scan_locale_dir"}
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
    assert len(counts) == 10, "expected 10 ported functions"


if __name__ == "__main__":
    main()