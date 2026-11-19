"""twin for hermes_cli/profile_describer.py:_extract_json_blob (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profile_describer.py"
    PY_FN = "_extract_json_blob"
    C_FN = "_extract_json_blob"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
