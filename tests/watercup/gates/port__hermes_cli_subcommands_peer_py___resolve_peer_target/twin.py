"""twin for hermes_cli/subcommands/peer.py:_resolve_peer_target (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/subcommands/peer.py"
    PY_FN = "_resolve_peer_target"
    C_FN = "_resolve_peer_target"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
