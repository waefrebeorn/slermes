"""twin for hermes_cli/subcommands/peer.py:_peer_key_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/subcommands/peer.py"
    PY_FN = "_peer_key_env"
    C_FN = "_peer_key_env"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
