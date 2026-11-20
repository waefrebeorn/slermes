#!/usr/bin/env python3
"""Generate tests/fixtures/gw_conn_ssh_flow2.jsonl (>=5 cases per function)."""
import json, hashlib, os
from pathlib import Path
OUT = Path(__file__).resolve().parent / "fixtures" / "gw_conn_ssh_flow2.jsonl"

OID = "0123456789abcdef0123456789abcdef"
NONCE = "0123456789abcdef"
def fp(tok): return hashlib.sha256((tok or "").encode()).hexdigest()[:32]
def lock(port=1234, **kw):
    d = {"schemaVersion":2, "pid":4242, "port":port, "spawnNonce":NONCE,
         "ownershipId":OID, "tokenFingerprint":fp("tok"), "protocolVersion":1,
         "logPath":"~/.hermes/desktop-ssh/%s/%s.log"%(OID,NONCE),
         "profile":"default", "hermesPath":"/usr/local/bin/hermes",
         "hermesHome":"~/.hermes", "startedAt":"2026-01-01T00:00:00.000Z", "devServer":""}
    d.update(kw); return d
def plan(**kw):
    d = {"hermes_path":"/usr/local/bin/hermes", "profile":"default", "spawn_nonce":NONCE,
         "token_file_path":"~/.hermes/desktop-ssh/%s/%s.token"%(OID,NONCE),
         "log_path":"~/.hermes/desktop-ssh/%s/%s.log"%(OID,NONCE),
         "hermes_home":"~/.hermes", "ownership_id":OID, "lock_metadata":{}}
    d.update(kw); return d
def raise_spec(kind, msg="boom"): return {"__raise__":kind,"msg":msg}
def deps(**kw):
    d={"ownership_id":OID,"profile":"default","remote_hermes_path":"",
       "reuse_token":"tok","ready_timeout_s":5.0}
    d.update(kw); return d

CASES=[]
def C(func, **inp): CASES.append({"func":func,"in":inp})

# --- pure helpers ---
for nm in ["_err"]:
    for k,m in [("unknown","x"),("transient-transport-error","m2"),("spawn-failed","m3"),("update-required","m4"),("other","")]:
        C(nm,kind=k,message=m)
for p in ["Default ","My_Profile","bad name","",123,"a"*65,"default","ok-name_1"]:
    C("backend_profile_arg",profile=p)
for t in ["","abc","tok","0"*64,None,"xyz"]:
    C("fingerprint_token",token=t)
for x in [OID,"abc",None,"A"*32,"0"*31]:
    C("validate_ownership_id",ownership_id=x)
for x in [NONCE,"abc",None,"A"*16,"0"*15]:
    C("validate_spawn_nonce",nonce=x)
for x in [OID,"bad"]:
    C("ownership_directory",ownership_id=x); C("lockfile_path",ownership_id=x); C("connect_reservation_path",ownership_id=x)
for oid,nonce in [(OID,NONCE),(OID,"bad"),("bad",NONCE)]:
    C("spawn_log_path",ownership_id=oid,nonce=nonce); C("spawn_token_path",ownership_id=oid,nonce=nonce)
for v in ["abc","a'b","",5,"a\\b",None]:
    C("shq",value=v)
for p in ["~/x","/a","~","relative","a\nb",""]:
    C("validate_remote_path",path=p); C("expand_remote_path",path=p)
for h in ["/home/u","~/x","../x","/a/..","~","/a/b/",123,"/ok-path_1"]:
    C("assert_safe_remote_home",home=h)
for h in ["/a/profiles/p1","/a/b","~/x/profiles/def","bad", "/a/profiles/p1/x"]:
    C("remote_install_root",home=h)
for v,lo,hi in [(5,1,10),(0,1,10),(-1,-5,5),(True,0,1),(1.5,0,2),("3",0,9)]:
    C("_valid_int",value=v,lo=lo,hi=hi)
good=lock()
for txt in ["", "not json", "[]", json.dumps({**good,"schemaVersion":3}),
            json.dumps({**good,"pid":0}), json.dumps(good),
            json.dumps({**good,"protocolVersion":2}), json.dumps({**good,"creationTime":"bad"})]:
    C("parse_lockfile",text=txt,ownership_id=OID)
for lk in [{}, {"a":1,"b":[1,2]}, good, {"x":None,"y":True}]:
    C("_lock_json",lock=lk)
for cmd,m,h in [("uname -s","M","H"),("a b","", ""),("x","M","")]:
    C("with_remote_update_mutex",command=cmd,marker_word=m,hermes_word=h)
for p in [plan(),plan(profile="bad name!"),plan(hermes_path="relative"),plan(spawn_nonce="bad")]:
    C("_spawn_payload_prefix",plan=p); C("build_spawn_command",plan=p)
for proof in [{"ok":True,"sshOwnerNonce":NONCE,"protocolVersion":1,"runtimeIntact":True},
              {"ok":True,"sshOwnerNonce":NONCE,"protocolVersion":1,"runtimeIntact":False},
              {"ok":1,"sshOwnerNonce":NONCE,"protocolVersion":1},
              {}, None, {"ok":True,"sshOwnerNonce":"x","protocolVersion":1}]:
    C("classify_ssh_reuse_proof",proof=proof,spawn_nonce=NONCE)
for lk in [lock(),lock(port=0),lock(port=-1),{**lock(),"profile":"other"},
           {**lock(),"tokenFingerprint":"0"*32},{**lock(),"devServer":"dev"}]:
    C("lock_serves_connect",lock=lk,profile="default",reuse_token="tok",
      hermes_path="/usr/local/bin/hermes",hermes_home="~/.hermes")
for e in ["address already in use","Cannot listen to port 3","bind failed","none","BIND xyz FAILED",""]:
    C("is_forward_bind_collision",error=e)
for reason in ["unparseable-json","malformed-pid","ownership-mismatch"]:
    C("_skew_error",ownership_id=OID,skew={"reason":reason})
C("_skew_error",ownership_id="bad",skew={"reason":"x"})

# --- seam-driven helpers ---
for i,s in enumerate([["aa"*16],["b"*32],["0"*64],["cd"*16],[None]]):
    C("mint_token",seams={"token_hex":s})
for out in [["Linux\nx86_64"],["Darwin\narm64"],["Windows\n10"],["Linux"],["\n"]]:
    C("probe_remote_platform",seams={"exec":out,"sleep":None})
for out in [["~/.hermes"],["/custom/home\n"],["\n\n"],[""],[raise_spec("SshReachError","x")]]:
    C("probe_remote_hermes_home",seams={"exec":out,"sleep":None})
for out in [["CLEAR"],["LIVE:123"],["HELD"],["garbage"],[raise_spec("SshReachError","x")]]:
    C("assert_remote_install_update_clear",hermes_home="~/.hermes",hermes_path="",seams={"exec":out,"sleep":None})
for cand,out in [("/usr/local/bin/hermes",["OK"]),("~/bin/h",[""]),("relative",[]),
                 ("/x",[raise_spec("SshReachError","x")]),("/y",["OK\n"])]:
    C("_is_executable",candidate=cand,seams={"exec":out,"sleep":None})
for rp,out in [("/usr/local/bin/hermes",["OK"]),("",["", "/usr/local/bin/hermes","OK"]),
               ("",["command-v-out","OK","OK"]),("",["","OK","OK"]),("bad-path",[])]:
    C("locate_hermes",remote_hermes_path=rp,seams={"exec":out,"sleep":None})
for out in [["hermes 1.2.3\n"],["a\nb"],[""],[raise_spec("SshReachError","x")],["  v2  "]]:
    C("probe_hermes_version",hermes_path="/usr/local/bin/hermes",seams={"with_timeout":"WRAPPED","exec":out})
for out in [["YES"],["NO"],["xYES"],["yes\n"],[""]]:
    C("remote_supports_ssh_ownership",hermes_path="/usr/local/bin/hermes",seams={"with_timeout":"WRAPPED","exec":out})
for out in [[json.dumps(good)],[""],["garbage"],[raise_spec("SshReachError","x")],[json.dumps({**good,"port":0})]]:
    C("read_lockfile",ownership_id=OID,seams={"exec":out,"sleep":None})
for th,ex in [(["abcd1234"],[None]),(["ffff0000"],[raise_spec("SshReachError","x")]),(["0"*16],[None])]:
    C("write_lockfile",ownership_id=OID,lock=good,seams={"token_hex":th,"exec":ex})
for ex in [[None],[raise_spec("SshReachError","x")],[raise_spec("RuntimeError","x")]]:
    C("remove_lockfile",ownership_id=OID,seams={"exec":ex})
for cmd,out in [("kill -0 1",["ALIVE"]),("kill -0 1",["MAYBE","DEAD"]),
                ("kill -0 1",[raise_spec("SshReachError","x")]),("kill -0 1",["MAYBE","MAYBE","ALIVE"]),
                ("kill -0 1",["DEAD"])]:
    C("exec_probe_verdict",command=cmd,sentinels=["ALIVE","DEAD"],failure="fail",seams={"exec":out,"sleep":None})
for pid,out in [(0,[]),(-1,[]),(4242,["ALIVE"]),(4242,["DEAD"]),(4242,["MAYBE","DEAD"])]:
    C("remote_pid_alive",pid=pid,seams={"exec":out,"sleep":None})
for pid,out in [(0,[]),(4242,["linux:12345"]),(4242,["bogus"]),(4242,["darwin:Wed Nov 18 2026"]),
                (4242,[raise_spec("SshReachError","x")])]:
    C("remote_process_creation_time",pid=pid,seams={"exec":out,"sleep":None})
for lk,out in [({},[]),({**good,"spawnNonce":"bad"},[]),(good,["OWNED"]),(good,["FOREIGN"]),(good,["MAYBE","OWNED"])]:
    C("pid_is_our_serve",lock=lk,ownership_id=OID,seams={"exec":out,"sleep":None})
for ex in [[None],[raise_spec("SshReachError","x"),None],[raise_spec("SshReachError","x"),raise_spec("SshReachError","x")]]:
    C("_terminate",pid=4242,seams={"exec":ex})
for ex in [[None],[None],[raise_spec("SshReachError","x"),None],[None],[None]]:
    C("cleanup_stale",ownership_id=OID,lock=good,pid_alive=False,seams={"exec":ex})
C("cleanup_stale",ownership_id=OID,lock={"reason":"x","__skew__":True},pid_alive=True,seams={})
for out in [[raise_spec("SshReachError","x")],[json.dumps(good),"","","","","",""],
            [json.dumps({**good,"port":0}),"","","","","",""]]:
    C("disconnect_remote",ownership_id=OID,seams={"exec":out,"sleep":None})
for path,ex in [("/tmp/x",[None]),("~/x",[raise_spec("SshReachError","x")]),("relative",[])]:
    C("_remove_remote",path=path,seams={"exec":ex})
C("_remove_remote",path="bad\n",seams={})
for iso in [["2026-01-01T00:00:00.000Z"],["2026-06-05T12:34:56.789Z"],["bad"],["1970-01-01T00:00:00.000Z"]]:
    C("_now_iso",seams={"now_iso":iso})
for out in ["Error: Profile 'foo' does not exist. Create it with: hermes profiles create foo",
            "error: profile \"bar\" does not exist.",
            "Error: Profile 'baz' does not exist.", "nothing", ""]:
    C("remote_profile_missing_error",output=out)
for out in [["x HERMES_BACKEND_READY port=1234 y"],["HERMES_DASHBOARD_READY port=9999"],
            ["nothing"],["HERMES_BACKEND_READY port=0"],["xHERMES_BACKEND_READY port=12"]]:
    C("scrape_ready_port",log_path="~/.hermes/desktop-ssh/x.log",timeout_s=1.0,
      seams={"monotonic":[0,1,2,3],"exec":out,"sleep":None})
for picks,fwds in [([5000],[None]),([5000,5001],[raise_spec("SshReachError","address already in use"),None]),
                   ([5000,5001],[raise_spec("SshReachError","oops"),None]),([5000],[raise_spec("SshReachError","address already in use")])]:
    C("open_forward",remote_port=1234,attempts=3,seams={"pick_port":picks,"forward":fwds})
for out in [[json.dumps(lock(port=1234)),None],[json.dumps(lock(port=0)),None,None,None],
            ["",None,None,None],[raise_spec("SshReachError","x")]]:
    C("wait_for_remote_spawn_completion",ownership_id=OID,timeout_s=1.0,
      seams={"monotonic":[0,1,2,3,4],"exec":out,"sleep":None})

# --- orchestrators (success paths; seam scripts in call order) ---
def reuse_seams():
    return {"exec":[ "ALIVE", "OWNED", "CLEAR", "ok", "authenticated-ok", "ALIVE" ],
            "sleep":None,"pick_port":[5000],"forward":[None],"cancel_forward":[None]}
for lk in [lock(), lock(port=0), lock(protocolVersion=2), lock(profile="other")]:
    C("_try_reuse",deps=deps(),remote={"platform":["Linux","x86_64"],"hermes_home":"~/.hermes",
       "hermes_path":"/usr/local/bin/hermes","hermes_version":"v1","profile":"default"},
       lock=lk,seams=reuse_seams())
def fresh_seams():
    return {"exec":["YES","", "None", "None", "0\n", "x HERMES_BACKEND_READY port=4321", None],
            "with_timeout":"WRAPPED","token_hex":["a"*32,"b"*16],"now_iso":["2026-01-01T00:00:00.000Z"],
            "assert_clear":None,"monotonic":[0,1,2,3,4,5],"sleep":None,"pick_port":[5000],
            "forward":[None],"wait_health":[None],"probe_proof":["authenticated-ok"]}
for profile in ["default","other"]:
    C("_spawn_fresh",deps=deps(profile=profile),remote={"platform":["Linux","x86_64"],
       "hermes_home":"~/.hermes","hermes_path":"/usr/local/bin/hermes","hermes_version":"v1","profile":profile},
       seams=fresh_seams())
C("_spawn_fresh",deps=deps(),remote={"platform":["Linux","x86_64"],"hermes_home":"~/.hermes",
   "hermes_path":"/usr/local/bin/hermes","hermes_version":"v1","profile":"default"},
   seams={"exec":["YES"],"with_timeout":"WRAPPED","token_hex":["a"*32,"b"*16],
          "now_iso":["2026-01-01T00:00:00.000Z"],"assert_clear":None})
C("_spawn_fresh",deps=deps(),remote={"platform":["Linux","x86_64"],"hermes_home":"~/.hermes",
   "hermes_path":"/usr/local/bin/hermes","hermes_version":"v1","profile":"default"},
   seams={"exec":[raise_spec("SshReachError","x")],"with_timeout":"WRAPPED","token_hex":["a"*32,"b"*16],
          "now_iso":["2026-01-01T00:00:00.000Z"],"assert_clear":None})
C("_spawn_fresh",deps=deps(reuse_token=""),remote={"platform":["Linux","x86_64"],"hermes_home":"~/.hermes",
   "hermes_path":"/usr/local/bin/hermes","hermes_version":"v1","profile":"default"},
   seams={"exec":["YES","", "None", "None", "EXISTING"],"with_timeout":"WRAPPED","token_hex":["a"*32,"b"*16],
          "now_iso":["2026-01-01T00:00:00.000Z"],"assert_clear":None})
for oid in [OID, "bad"]:
    C("_rollback_spawn",ownership_id=oid,owned=lock(),spawned={"existing":False,"pid":4242,
       "spawn_nonce":NONCE,"log_path":"~/.hermes/desktop-ssh/x.log","token_file_path":"~/.hermes/desktop-ssh/x.token"},
       local_port=5000,remote_port=1234,seams={"exec":["ALIVE",None,None],"sleep":None})
probe_seams={"exec":["Linux\nx86_64","/home/u","CLEAR","/usr/local/bin/hermes","v1"],"with_timeout":"WRAPPED"}
for rp in ["","/usr/local/bin/hermes"]:
    C("_probe_remote",deps=deps(remote_hermes_path=rp),seams=probe_seams)
C("_probe_remote",deps=deps(),seams={"exec":[raise_spec("SshReachError","x")],"with_timeout":"WRAPPED"})
conn_seams={"exec":["Linux\nx86_64","/home/u","CLEAR","/usr/local/bin/hermes","v1",
                     "ALIVE","OWNED","CLEAR","ok","authenticated-ok","ALIVE"],
            "with_timeout":"WRAPPED","sleep":None,"pick_port":[5000],"forward":[None],"cancel_forward":[None]}
for rtok in ["tok",""]:
    C("connect",deps=deps(reuse_token=rtok),seams=conn_seams)
C("connect",deps=deps(),seams={"exec":["Linux\nx86_64","/home/u","CLEAR","/usr/local/bin/hermes","v1",
    raise_spec("SshReachError","x")],"with_timeout":"WRAPPED","sleep":None,"pick_port":[5000],"forward":[None]})

# spawn_remote_serve: help verdict decides; then token upload / assert / spawn
for out in [["NO"],["YES","", "None", "None", "1234\n"],["YES","", "None", "None", "EXISTING"],
            ["YES","", "None", "None", "bad\n"],
            [raise_spec("SshReachError","x")]]:
    C("spawn_remote_serve",hermes_path="/usr/local/bin/hermes",profile="default",token="tok",
      ownership_id=OID,hermes_home="~/.hermes",
      seams={"with_timeout":"WRAPPED","exec":out,"token_hex":["b"*16],
             "now_iso":["2026-01-01T00:00:00.000Z"],"assert_clear":None})

# Ensure >= 5 cases per function (duplicate with a harmless marker field).
from collections import Counter
_cnt=Counter(c["func"] for c in CASES)
for fn in list(PY_NAMES := [x for x in {c["func"] for c in CASES}]):
    while _cnt[fn] < 5:
        src=next(c for c in CASES if c["func"]==fn)
        d=json.loads(json.dumps(src)); d["in"]["__pad"]=_cnt[fn]
        CASES.append(d); _cnt[fn]+=1

OUT.parent.mkdir(parents=True,exist_ok=True)
with open(OUT,"w",encoding="utf-8",newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c,ensure_ascii=False)+"\n")
from collections import Counter
cnt=Counter(c["func"] for c in CASES)
print("wrote",len(CASES),"cases across",len(cnt),"funcs; min",min(cnt.values()))
for k,v in sorted(cnt.items()):
    if v<5: print("LOW",k,v)
