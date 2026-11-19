"""twin for gateway/relay/command_manifest.py:build_relay_command_manifest (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/command_manifest.py"
    PY_FN = "build_relay_command_manifest"
    C_FN = "gateway_relay_command_manifest_build_relay_command_manifest"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
