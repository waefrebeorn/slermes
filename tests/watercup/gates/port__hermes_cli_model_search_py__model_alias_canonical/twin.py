"""twin for hermes_cli/model_search.py:model_alias_canonical."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_hermes_cli_model_search_py", os.path.join(DEV, "hermes_cli/model_search.py"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_hermes_cli_model_search_py"] = _pb
_spec.loader.exec_module(_pb)


def trace(inp):
    c = Cup("model_alias_canonical")
    try:
        result = _pb.model_alias_canonical(inp)
        c.valstr(str(result))
    except Exception as e:
        c.valstr(f"ERROR:{type(e).__name__}")
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]
