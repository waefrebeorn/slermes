"""twin for hermes_cli/subcommands/peer.py:_peer_secret (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/subcommands/peer.py"
    PY_FN = "_peer_secret"
    C_FN = "_peer_secret"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
