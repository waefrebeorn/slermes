"""twin for tools/skill_usage.py:_read_bundled_manifest_names (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_usage.py"
    PY_FN = "_read_bundled_manifest_names"
    C_FN = "_read_bundled_manifest_names"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
