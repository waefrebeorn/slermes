"""twin for hermes_cli/plugins.py:_VALID_PLUGIN_KINDS (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_VALID_PLUGIN_KINDS"
    C_FN = "plugin_manifest_kind_is_valid"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
