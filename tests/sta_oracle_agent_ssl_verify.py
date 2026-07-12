#!/usr/bin/env python3
"""Oracle: agent/ssl_verify.py vs LIVE Python (newline-delimited JSON)."""
import json, sys, os, types
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util
spec = importlib.util.spec_from_file_location("sv_mod", "/home/wubu/hermes-agent-dev/agent/ssl_verify.py")
mod = importlib.util.module_from_spec(spec)
# only stub 'utils' if needed; use real 'ssl'
sys.modules.setdefault("utils", __import__("types").ModuleType("utils"))
try:
    spec.loader.exec_module(mod)
except Exception as e:
    print("IMPORT_FAIL", repr(e)); sys.exit(2)

mm = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    b = json.loads(line)
    if b["t"] == "coerce":
        exp = bool(mod._coerce_insecure(b["in"])); got = bool(b["out"])
        if exp != got: mm += 1; print(f"MISMATCH coerce {b['in']!r}: exp {exp} got {got}")
    elif b["t"] == "resolve":
        for e in ["HERMES_CA_BUNDLE","SSL_CERT_FILE","REQUESTS_CA_BUNDLE","CURL_CA_BUNDLE"]:
            os.environ.pop(e, None)
        capath = None
        if b["ca"]:
            capath = b["ca"]
            # create a valid self-signed cert so SSLContext() succeeds
            import subprocess
            try:
                subprocess.run(["openssl","req","-x509","-newkey","rsa:2048","-keyout","/tmp/_k.pem",
                                "-out",capath,"-days","1","-nodes","-subj","/CN=test"],
                               capture_output=True, check=True)
            except Exception:
                open(capath,"w").write("x")
        exp = mod.resolve_httpx_verify(ca_bundle=capath, ssl_verify=b["sv"] if b["sv"] else None)
        if exp is False: exps = "false"
        elif exp is True: exps = "true"
        else: exps = "ca:" + (capath or "")
        got = b["out"]
        if exps != got: mm += 1; print(f"MISMATCH resolve ca={b['ca'][:20]} sv={b['sv']}: exp {exps} got {got}")
        if capath: 
            try: os.remove(capath)
            except OSError: pass
if mm:
    print(f"oracle: {mm} mismatch(es)"); sys.exit(1)
print("oracle: 0 mismatches")
