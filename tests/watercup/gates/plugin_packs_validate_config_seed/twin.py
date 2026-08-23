"""twin for plugin_packs_validate_config_seed.

Imports LIVE hermes_cli/plugin_packs.py::validate_config_seed and traces the
same rung order as the C harness (which links the REAL port
plugin_packs_pure.c::plugin_pack_validate_config_seed).

Input format per cup: "<plugin_id><TAB><seed_json>" (fields without tabs).

Rung order:
  B1 isinstance(seed, dict)?
  per key (insertion order): B2 reserved/allow_? -> B3 secret-shaped?
  clean pass -> V "ok"; rejection -> V(the PackError message, which is exactly
  what the C port returns as {"error": ...}).
"""
import os, sys
import json as _jsonmod
import importlib
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

spec = ilu.spec_from_file_location(
    "wc_live_plugin_packs", os.path.join(DEV, "hermes_cli", "plugin_packs.py"))
pp = importlib.util.module_from_spec(spec)
sys.modules["wc_live_plugin_packs"] = pp
spec.loader.exec_module(pp)


def _secret_shaped(key):
    # LIVE: _SECRET_KEY_RE.search(key) with (?i)(token|secret|passw(or)?d|
    # api[_-]?key|private[_-]?key|credential|auth)
    return bool(pp._SECRET_KEY_RE.search(key))


def _verdict_json(err):
    """The C port returns rejections as {"error": "<message>"} (compact,
    raw UTF-8); mirror that canonical verdict string here."""
    return _jsonmod.dumps({"error": err}, ensure_ascii=False,
                          separators=(",", ":"))


def trace(plugin_id, seed):
    c = Cup("validate_config_seed")
    try:
        pp.validate_config_seed(plugin_id, seed)
        ok = True
        err = None
    except pp.PackError as e:
        ok = False
        err = str(e)
    except TypeError:
        # non-dict seeds raise PackError inside LIVE too; guard anyway
        ok = False
        err = "Pack config for plugin '%s' must be a mapping of plugins.entries.%s keys." % (plugin_id, plugin_id)

    is_dict = isinstance(seed, dict)
    c.branch(not is_dict)                    # B1 (true = NOT an object)
    stopped = False
    if is_dict:
        for key in seed:                     # insertion order, like the port
            if key in pp._RESERVED_ENTRY_KEYS or key.startswith("allow_"):
                c.branch(True)               # B2
                c.valstr(_verdict_json(err))         # V2
                stopped = True
                break
            c.branch(False)
            if _secret_shaped(key):
                c.branch(True)               # B3
                c.valstr(_verdict_json(err))         # V3
                stopped = True
                break
            c.branch(False)
    if not stopped and is_dict:
        c.valstr("ok")                       # V4
    if not is_dict:
        c.valstr(_verdict_json(err))         # V1
    assert ok == (is_dict and not stopped), "LIVE verdict disagrees with traced ladder"
    c.finalize()
    return c


def run(inputs):
    cups = []
    import json as _json
    for raw in inputs:
        plugin_id, seed_json = raw.split("\t", 1)
        cups.append(trace(plugin_id, _json.loads(seed_json)).finalize())
    return cups
