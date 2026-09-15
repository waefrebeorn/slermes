"""twin for gateway/status.py:_build_runtime_status_record."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_gateway_status_py", os.path.join(DEV, "gateway/status.py"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_gateway_status_py"] = _pb
_spec.loader.exec_module(_pb)


def trace(inp):
    c = Cup("_build_runtime_status_record")
    try:
        result = _pb._build_runtime_status_record(inp)
        c.valstr(str(result))
    except Exception as e:
        c.valstr(f"ERROR:{type(e).__name__}")
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]
