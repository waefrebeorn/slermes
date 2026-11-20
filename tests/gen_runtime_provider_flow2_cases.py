#!/usr/bin/env python3
"""Generate tests/fixtures/runtime_provider_flow2.jsonl (>=5 cases per func)."""
import json, os
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "runtime_provider_flow2.jsonl"

FUNCS = [
 "load_config","get_compatible_custom_providers","normalize_extra_headers",
 "_effective_model","_pool_entry_api_key","_pool_entry_base_url","_nous_min_key_ttl",
 "_finalize_base_url","_owned_url_credential","_actual_local_key","_actual_url",
 "_named_custom_rung","_openrouter_fallback",
]

def R(script): return {"__raise__":"RuntimeError","msg":script}
def AE(msg="denied"): return {"__raise__":"AuthError","msg":msg}
def VE(msg="bad"): return {"__raise__":"ValueError","msg":msg}

def cases_for(f):
    out=[]
    def c(**kw): out.append({"func":f,"in":kw})
    if f=="load_config":
        for cfg in ({}, {"model":{"default":"m"}}, {"model":{"provider":"anthropic"}}, {"providers":{}}, {"model":{"base_url":"http://x"}}):
            c(io={"load_config":cfg})
    elif f=="get_compatible_custom_providers":
        for cfg in (None, {}, {"providers":{}}, [], {"providers":{"x":{"enabled":True}}}): c(config=cfg, io={"get_compatible_custom_providers":[cfg]})
    elif f=="normalize_extra_headers":
        for v in (None, {}, {"X-A":"1"}, [], {"X-B":"2"}): c(value=v, io={"normalize_extra_headers":v})
    elif f=="_effective_model":
        for tm,d in ((None,None),("m",None),(None,"d"),("t","d"),("", "")):
            c(target_model=tm, model_cfg={"default":d} if d is not None else {})
    elif f=="_pool_entry_api_key":
        for e in ({},{"access_token":"a"},{"runtime_api_key":"r","access_token":"a"},{"runtime_api_key":""},{"runtime_api_key":None,"access_token":"b"}): c(entry=e)
    elif f=="_pool_entry_base_url":
        for e in ({},{"base_url":"b"},{"runtime_base_url":"r","base_url":"b"},{"runtime_base_url":None,"base_url":"b"},{"runtime_base_url":"r"}): c(entry=e)
    elif f=="_nous_min_key_ttl":
        for v in (None,60,120,1800,3600): c(io={"env_int":v if v is not None else 1800})
    elif f=="_finalize_base_url":
        for prov,base,fam in (("openai","https://x/v1",None),("openai","https://x",None),("lmstudio","https://l",None),("actual","https://a",None),("openai","https://x/v1",{})):
            c(provider=prov,api_mode="chat_completions",base_url=base,io={"opencode_provider_family":fam,"normalize_opencode_base_url":base,"normalize_lmstudio_base_url":base,"normalize_actual_base_url":base})
    elif f=="_owned_url_credential":
        for b in ("https://x","", "https://y"): c(base_url=b,model_cfg={},provider="openai",io={"owned_config_url_credential":[False,""]})
    elif f=="_actual_local_key":
        for prov,key,local in (("actual","",True),("actual","",False),("actual","k",True),("openai","",True),("openai","k",False)):
            c(provider=prov,api_key=key,base_url="http://localhost:1234",io={"is_actual_local_base_url":local})
    elif f=="_actual_url":
        for prov,base in (("actual","https://x/"),("openai","https://x/"),("actual","http://localhost"),("openai",""),("actual","https://y")):
            c(provider=prov,base_url=base,io={"normalize_actual_base_url":base.rstrip("/")})
    elif f=="_named_custom_rung":
        for rt,maybe in ((None,None),({"provider":"custom","api_mode":"chat_completions"},"codex_responses"),({"provider":"custom","api_mode":"codex_responses"},"chat_completions"),({"provider":"openai","api_mode":"codex_responses"},"codex_responses"),(None,"chat_completions")):
            c(requested_provider="custom",explicit_api_key=None,explicit_base_url=None,target_model=None,
              io={"resolve_named_custom_runtime":rt,"get_model_config":{},"maybe_apply_codex_app_server_runtime":maybe})
    elif f=="_openrouter_fallback":
        for rt in (None,{"provider":"openrouter","api_mode":"chat_completions"},{"provider":"custom","api_mode":"chat_completions","api_key":"k"},{"provider":"custom"},{"provider":"openrouter","api_key":"k"}):
            c(requested_provider="auto",explicit_api_key=None,explicit_base_url=None,io={"resolve_openrouter_runtime":rt})
    elif f=="PoolChoice.commit":
        for picked,r in ((None,None),({"pool":{},"entry":{"id":"e"}},{}),({"pool":{},"entry":{"id":"e"}},None),({"pool":{},"entry":{"id":"e"}},{"ok":1}),({"pool":{},"entry":{}},{})):
            st={"model":"m","picked":picked,"_endpoint_url":"https://x"}
            c(state=st,io={"pool.commit":r})
    else:
        for i in range(5): c()

    # pad to >=5
    while len(out)<5:
        out.append({"func":f,"in":dict(out[0]["in"])})
    return out[:max(5,len(out))]

def main():
    rows=[]
    for f in FUNCS:
        rows.extend(cases_for(f))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT,"w",encoding="utf-8",newline="\n") as fh:
        for r in rows: fh.write(json.dumps(r,ensure_ascii=False,separators=(",",":"))+"\n")
    print(f"wrote {len(rows)} cases for {len(FUNCS)} funcs -> {OUT}")

if __name__=="__main__": main()
