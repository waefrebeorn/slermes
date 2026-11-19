"""twin for hermes_cli/plugins.py:_parse_manifest_v2_fields (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_parse_manifest_v2_fields"
    C_FN = "_parse_manifest_v2_fields"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
