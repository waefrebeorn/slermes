"""twin for hermes_cli/plugins.py:get_plugin_toolset_keys_nowait."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_hermes_cli_plugins_py", os.path.join(DEV, "hermes_cli/plugins.py"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_hermes_cli_plugins_py"] = _pb
_spec.loader.exec_module(_pb)


def trace(inp):
    c = Cup("get_plugin_toolset_keys_nowait")
    try:
        result = _pb.get_plugin_toolset_keys_nowait(inp)
        c.valstr(str(result))
    except Exception as e:
        c.valstr(f"ERROR:{type(e).__name__}")
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]
