"""twin for hermes_cli/archive_safe.py:copy_regular_files."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_hermes_cli_archive_safe_py", os.path.join(DEV, "hermes_cli/archive_safe.py"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_hermes_cli_archive_safe_py"] = _pb
_spec.loader.exec_module(_pb)


def trace(inp):
    c = Cup("copy_regular_files")
    try:
        result = _pb.copy_regular_files(inp)
        c.valstr(str(result))
    except Exception as e:
        c.valstr(f"ERROR:{type(e).__name__}")
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]
