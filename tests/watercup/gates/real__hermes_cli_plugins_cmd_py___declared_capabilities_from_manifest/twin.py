"""twin for hermes_cli/plugins_cmd.py:_declared_capabilities_from_manifest (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins_cmd.py"
    PY_FN = "_declared_capabilities_from_manifest"
    C_FN = "_declared_capabilities_from_manifest"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
