"""twin for hermes_cli/plugins.py:discover_entrypoint_manifests (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "discover_entrypoint_manifests"
    C_FN = "discover_entrypoint_manifests"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
