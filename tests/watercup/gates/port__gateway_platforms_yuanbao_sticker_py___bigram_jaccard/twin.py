"""twin for gateway/platforms/yuanbao_sticker.py:_bigram_jaccard (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao_sticker.py"
    PY_FN = "_bigram_jaccard"
    C_FN = "cli_gateway_platforms_yuanbao_sticker__bigram_jaccard"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
