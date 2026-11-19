"""twin for agent/skill_bundles.py:resolve_bundle_command_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_bundles.py"
    PY_FN = "resolve_bundle_command_key"
    C_FN = "resolve_bundle_command_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
