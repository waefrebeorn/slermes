#!/usr/bin/env python3
"""Oracle for kanban_plugin_api_flow2 (plugins/kanban/dashboard/plugin_api.py,
the 78 UNCLAIMED REAL_GAP features). Reads harness JSONL on stdin, computes the
expected {ret,trace,state} with a Python transcription of the C port's decision
logic, and prints 'ORACLE kanban_plugin_api_flow2: N cases, M mismatches'.

The oracle is a literal Python mirror of src/plugins/kanban/dashboard/
kanban_plugin_api_flow2.c: same seam names, same arg projections, same ordered
trace, same exception hierarchy, same result assembly. Seam scripts come from
in.io (name -> {"ret":...} / {"raise":{...}} / a per-call queue). Missing scripts
fail closed with RuntimeError "no script for <name>" (recorded, then raised).
"""
import json, sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# ---- full builtin exception hierarchy (mirror of kpa2_exc_parent) -----------
_PARENT = {
 "BaseException":None,"Exception":"BaseException",
 "SystemExit":"BaseException","KeyboardInterrupt":"BaseException","GeneratorExit":"BaseException",
 "CancelledError":"BaseException","asyncio.CancelledError":"BaseException",
 "ArithmeticError":"Exception","FloatingPointError":"ArithmeticError","OverflowError":"ArithmeticError",
 "ZeroDivisionError":"ArithmeticError","AssertionError":"Exception","AttributeError":"Exception",
 "BufferError":"Exception","EOFError":"Exception","ImportError":"Exception","ModuleNotFoundError":"ImportError",
 "LookupError":"Exception","IndexError":"LookupError","KeyError":"LookupError","MemoryError":"Exception",
 "TypeError":"Exception","SystemError":"Exception","StopIteration":"Exception","StopAsyncIteration":"Exception",
 "NameError":"Exception","UnboundLocalError":"NameError","ReferenceError":"Exception",
 "OSError":"Exception","IOError":"Exception","EnvironmentError":"Exception","BlockingIOError":"OSError",
 "ChildProcessError":"OSError","FileExistsError":"OSError","FileNotFoundError":"OSError","InterruptedError":"OSError",
 "IsADirectoryError":"OSError","NotADirectoryError":"OSError","PermissionError":"OSError",
 "ProcessLookupError":"OSError","TimeoutError":"OSError","ConnectionError":"OSError",
 "BrokenPipeError":"ConnectionError","ConnectionAbortedError":"ConnectionError",
 "ConnectionRefusedError":"ConnectionError","ConnectionResetError":"ConnectionError",
 "RuntimeError":"Exception","NotImplementedError":"RuntimeError","RecursionError":"RuntimeError",
 "SyntaxError":"Exception","IndentationError":"SyntaxError","TabError":"IndentationError",
 "UnicodeError":"ValueError","UnicodeDecodeError":"UnicodeError","UnicodeEncodeError":"UnicodeError",
 "UnicodeTranslateError":"UnicodeError","ValueError":"Exception","HTTPException":"Exception",
 "_StatusRejected":"Exception","_SetupGoBack":"Exception","Warning":"Exception","UserWarning":"Warning",
 "DeprecationWarning":"Warning","PendingDeprecationWarning":"Warning","SyntaxWarning":"Warning",
 "RuntimeWarning":"Warning","FutureWarning":"Warning","ImportWarning":"Warning","UnicodeWarning":"Warning",
 "BytesWarning":"Warning","ResourceWarning":"Warning",
}
def exc_is(t, wanted):
    if not t or not wanted: return False
    if t == wanted: return True
    cur = t
    for _ in range(24):
        cur = _PARENT.get(cur, "Exception")
        if cur == wanted: return True
        if cur == "BaseException": break
    return False

def raised(v): return isinstance(v,dict) and "__raise__" in v
def rtype(v): return v.get("__raise__","") if isinstance(v,dict) else ""
def rmsg(v): return v.get("message","") if isinstance(v,dict) else ""
def make_raise(t,m): return {"__raise__":t,"message":m if m is not None else ""}
def http_exc(status,detail):
    d = detail if detail is not None else ""
    return {"__raise__":"HTTPException","message":f"{status}: {d}","status_code":status,"detail":d}

def truthy(v):
    if v is None: return False
    if isinstance(v,bool): return v
    if isinstance(v,(int,float)): return v!=0
    if isinstance(v,str): return len(v)>0
    if isinstance(v,(list,dict)): return len(v)>0
    return False

def strip(s): return (s or "").strip()
def g(o,k,d=None):
    if isinstance(o,dict): return o.get(k,d)
    return d
def sval(o,k):
    v = g(o,k)
    return v if isinstance(v,str) else ""

class Ctx:
    def __init__(self, inp):
        self.io = g(inp,"io",{}) or {}
        self.cur = {}
        self.trace = []
    def call(self, seam_name, **args):
        self.trace.append(dict(call=seam_name, **args))
        sc = self.io.get(seam_name)
        if sc is None:
            return make_raise("RuntimeError", f"no script for {seam_name}")
        if isinstance(sc,list):
            i = self.cur.get(seam_name,0); self.cur[seam_name]=i+1
            sc = sc[i] if i < len(sc) else None
        if isinstance(sc,dict) and "raise" in sc:
            spec=sc["raise"]; e=make_raise(spec.get("type","Exception"),spec.get("message",""))
            for k in ("status_code","detail","code","name"):
                if k in spec: e[k]=spec[k]
            return e
        if isinstance(sc,dict) and "ret" in sc: return json.loads(json.dumps(sc["ret"]))
        return json.loads(json.dumps(sc))
    def discard(self,v):
        if raised(v): return v
        return None
    def callvoid(self,seam_name,**args): return self.discard(self.call(seam_name,**args))



# ---- mirrors of kpa2_* handlers --------------------------------------------
def m_board_conn(c,i):
    b=c.call("_resolve_board",board=g(i,"board"))
    if raised(b): return b
    conn=c.call("_conn",board=json.loads(json.dumps(b)))
    if raised(conn): return conn
    x=c.discard(c.call("conn.close",conn=json.loads(json.dumps(conn))))
    ret=[b if b is not None else None,conn]
    return x if x is not None else ret

def m_with_board_pinned(c,i):
    b=c.call("_resolve_board",board=g(i,"board"))
    if raised(b): return b
    board = str(b) if truthy(b) else "default"
    e=c.call("kanban_db.scoped_current_board",board=board)
    if raised(e): return e
    r=c.call(g(i,"fn","fn"),board=board)
    x=c.discard(c.call("scoped_current_board.exit"))
    if raised(r): return r
    return x if x is not None else r

def m_run_aux(c,i):
    b=c.call("_resolve_board",board=g(i,"board"))
    if raised(b): return b
    board=str(b) if truthy(b) else "default"
    e=c.call("kanban_db.scoped_current_board",board=board)
    if raised(e): return e
    mod="hermes_cli."+str(g(i,"module",""))
    m=c.call("importlib.import_module",module=mod)
    if raised(m): r=m
    else:
        a=g(i,"author")
        r=c.call(g(i,"fn","fn"),task_id=g(i,"task_id"),author=(json.loads(json.dumps(a)) if truthy(a) else None))
    x=c.discard(c.call("scoped_current_board.exit"))
    if raised(r): return r
    return x if x is not None else r

def m_map_errors(c,i):
    body=c.call("body")
    if not raised(body): return body
    t=rtype(body); msg=rmsg(body)
    types=g(i,"types",[]) or []
    match=any(exc_is(t,w) for w in types if isinstance(w,str))
    if not match: return make_raise(t,"")
    return http_exc(int(g(i,"status",400)),msg)

def m_errors_to_500(c,i):
    body=c.call("body")
    if not raised(body): return body
    t=rtype(body)
    if exc_is(t,"HTTPException") or not exc_is(t,"Exception"): return body
    return http_exc(500,f"{g(i,'prefix','')}: {rmsg(body)}")

def m_parents_blocking_ready(c,i):
    rows=c.call("conn.execute",sql="SELECT t.id, t.title, t.status FROM tasks t JOIN task_links l ON l.parent_id = t.id WHERE l.child_id = ? AND t.status != 'done'",args=[g(i,"task_id")])
    if raised(rows): return rows
    out=[]
    if isinstance(rows,list):
        for r in rows: out.append({"id":g(r,"id"),"title":g(r,"title"),"status":g(r,"status")})
    return out

def m_configured_home_channels(c,i):
    m=c.call("importlib.import_module",module="gateway.config")
    if raised(m): return []
    cfg=c.call("load_gateway_config")
    if raised(cfg): return []
    out=[]
    plats=g(cfg,"platforms",{}) or {}
    if isinstance(plats,dict):
        for pc in plats.values():
            hc=g(pc,"home_channel")
            if not truthy(hc): continue
            out.append({"platform":g(pc,"value"),"chat_id":g(hc,"chat_id"),
                        "thread_id":json.loads(json.dumps(g(hc,"thread_id"))) if truthy(g(hc,"thread_id")) else "",
                        "name":json.loads(json.dumps(g(hc,"name"))) if truthy(g(hc,"name")) else "Home"})
    out.sort(key=lambda r: str(g(r,"platform","")))
    return out

def bc_open(c,i):
    b=c.call("_resolve_board",board=g(i,"board"))
    if raised(b): return (b,None,None)
    conn=c.call("_conn",board=json.loads(json.dumps(b)))
    if raised(conn): return (conn,None,None)
    return (None,b,conn)
def bc_close(c,conn): return c.discard(c.call("conn.close",conn=json.loads(json.dumps(conn))))

def m_get_task(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    rst=g(i,"run_state_type"); rsn=g(i,"run_state_name")
    if truthy(rst)!=truthy(rsn): e=http_exc(400,"run_state_type and run_state_name must be passed together or omitted")
    elif truthy(rst) and str(rst) not in ("status","outcome"): e=http_exc(400,"run_state_type must be 'status' or 'outcome'")
    if e: 
        x=bc_close(c,conn)
        return e
    task=c.call("_require_task",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    if raised(task): e=task
    else:
        summary=c.call("kanban_db.latest_summary",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
        rs=c.call("kanban_db.current_run_started_ats",conn=json.loads(json.dumps(conn)),ids=[g(i,"task_id")])
        td=c.call("_task_dict",task=task,latest_summary=(None if raised(summary) else json.loads(json.dumps(summary))),
                  current_run_started_at=(None if raised(rs) else g(rs,str(g(i,"task_id","")),None)))
        links=c.call("_links_for",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
        if raised(links): e=links
        else:
            children=g(links,"children",[]) or []
            sums=c.call("kanban_db.latest_summaries",conn=json.loads(json.dumps(conn)),ids=json.loads(json.dumps(children)))
            diags=c.call("_compute_task_diagnostics",conn=json.loads(json.dumps(conn)),task_ids=[g(i,"task_id")])
            dt=None if raised(diags) else g(diags,str(g(i,"task_id","")),None)
            att=c.call("_attach_diagnostics",task_d=td,diags=dt if dt is not None else [])
            comments=c.call("kanban_db.list_comments",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
            events=c.call("kanban_db.list_events",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
            atts=c.call("kanban_db.list_attachments",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
            lts=c.call("_link_tasks",conn=json.loads(json.dumps(conn)),links=json.loads(json.dumps(links)))
            runs=c.call("kanban_db.list_runs",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
            e={"task":td,"comments":None if raised(comments) else json.loads(json.dumps(comments)),
               "events":None if raised(events) else json.loads(json.dumps(events)),
               "attachments":None if raised(atts) else json.loads(json.dumps(atts)),
               "links":links,"link_tasks":lts,"child_results":[],
               "runs":None if raised(runs) else json.loads(json.dumps(runs))}
    x=bc_close(c,conn)
    if x is not None and not raised(e): e=x
    return e

def m_list_task_attachments(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    e=c.callvoid("_require_task",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    atts=None
    if not e: atts=c.call("kanban_db.list_attachments",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    x=bc_close(c,conn)
    if e: return e
    if x is not None: return x
    return {"attachments":None if raised(atts) else json.loads(json.dumps(atts))}

def m_remove_attachment(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    r=c.call("kanban_db.delete_attachment",conn=json.loads(json.dumps(conn)),attachment_id=g(i,"attachment_id"))
    if not raised(r) and not truthy(r): r=http_exc(404,"attachment not found")
    x=bc_close(c,conn)
    if raised(r): return r
    return {"ok":True,"id":g(i,"attachment_id")}

def m_delete_task(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    r=c.call("kanban_db.delete_task",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    if not raised(r) and not truthy(r): r=http_exc(404,f"task {g(i,'task_id','')} not found")
    x=bc_close(c,conn)
    if raised(r): return r
    return {"deleted":True,"task_id":g(i,"task_id")}

def m_add_link(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    r=c.call("kanban_db.link_tasks",conn=json.loads(json.dumps(conn)),parent_id=g(i,"parent_id"),child_id=g(i,"child_id"))
    x=bc_close(c,conn)
    if raised(r): return r
    return {"ok":True,"gated":json.loads(json.dumps(r))}

def m_delete_link(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    r=c.call("kanban_db.unlink_tasks",conn=json.loads(json.dumps(conn)),parent_id=g(i,"parent_id"),child_id=g(i,"child_id"))
    x=bc_close(c,conn)
    if raised(r): return r
    return {"ok":truthy(r)}

def m_add_comment(c,i):
    body=str(g(g(i,"payload",{}),"body",""))
    if not strip(body): return http_exc(400,"body is required")
    e,b,conn=bc_open(c,i)
    if e: return e
    e=c.callvoid("_require_task",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    if not e: e=c.callvoid("kanban_db.add_comment",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    x=bc_close(c,conn)
    if e: return e
    return {"ok":True}

def bc_core(c,i,core,args):
    e,b,conn=bc_open(c,i)
    if e: return e
    r=c.call(core,**({} if args is None else args))
    x=bc_close(c,conn)
    if x is not None and not raised(r): return x
    return r

def m_get_stats(c,i): return bc_core(c,i,"kanban_db.board_stats",g(i,"args"))
def m_get_assignees(c,i):
    r=bc_core(c,i,"kanban_db.known_assignees",g(i,"args"))
    if raised(r): return r
    return {"assignees":r}
def m_get_run_endpoint(c,i):
    r=bc_core(c,i,"_require_run",g(i,"args"))
    if raised(r): return r
    return {"run":r}
def m_get_home_channels(c,i):
    homes=c.call("_configured_home_channels")
    if raised(homes): return homes
    if truthy(g(i,"task_id")):
        e,b,conn=bc_open(c,i)
        if e: return e
        e=c.callvoid("kbn.list_notify_subs",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
        x=bc_close(c,conn)
        if e: return e
    return homes
def m_home_for_platform(c,i):
    homes=c.call("_configured_home_channels")
    if raised(homes): return homes
    want=str(g(i,"platform",""))
    if isinstance(homes,list):
        for h in homes:
            if str(g(h,"platform",""))==want: return json.loads(json.dumps(h))
    return http_exc(404,g(i,"detail",""))
def m_subscribe_home(c,i):
    home=c.call("_home_for_platform",platform=g(i,"platform"))
    if raised(home): return home
    e,b,conn=bc_open(c,i)
    if e: return e
    e=c.callvoid("_require_task",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    if not e: e=c.callvoid("kbn.add_notify_sub",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"),platform=g(i,"platform"),chat_id=g(home,"chat_id"))
    x=bc_close(c,conn)
    if e: return e
    return {"ok":True,"task_id":g(i,"task_id"),"home_channel":home}
def m_unsubscribe_home(c,i):
    home=c.call("_home_for_platform",platform=g(i,"platform"))
    if raised(home): return home
    e,b,conn=bc_open(c,i)
    if e: return e
    e=c.callvoid("kbn.remove_notify_sub",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"),platform=g(i,"platform"),chat_id=g(home,"chat_id"))
    x=bc_close(c,conn)
    if e: return e
    return {"ok":True,"task_id":g(i,"task_id"),"home_channel":home}
def m_get_task_log(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    e=c.callvoid("_require_task",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    content=None
    if not e: content=c.call("kanban_db.read_worker_log",task_id=g(i,"task_id"),tail_bytes=g(i,"tail"))
    if e:
        bc_close(c,conn); return e
    sz=c.call("log_path.stat",task_id=g(i,"task_id"))
    x=bc_close(c,conn)
    r={"task_id":g(i,"task_id"),"path":g(i,"log_path"),"exists":content is not None,
       "size_bytes":None if raised(sz) else json.loads(json.dumps(sz)),
       "content":"" if content is None else json.loads(json.dumps(content)),"truncated":False}
    return r


def m_dispatch(c,i): return bc_core(c,i,"kbd.dispatch_once",g(i,"args"))
def m_terminate_run(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    r=c.call("_require_run",conn=json.loads(json.dumps(conn)),run_id=g(i,"run_id"))
    if raised(r):
        bc_close(c,conn); return r
    if truthy(g(r,"ended_at","")):
        r=http_exc(409,f"run {g(i,'run_id','')} already ended")
    if not raised(r):
        ok=c.call("kanban_db.reclaim_task",conn=json.loads(json.dumps(conn)),task_id=g(r,"task_id"))
        if not truthy(ok):
            r=http_exc(409,f"cannot terminate run {g(i,'run_id','')}: task {g(r,'task_id','')} is no longer in a reclaimable state")
        else:
            r={"ok":True,"run_id":g(i,"run_id"),"task_id":g(r,"task_id")}
    x=bc_close(c,conn)
    if x is not None and not raised(r): return x
    return r
def m_reclaim_task(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    r=c.call("kanban_db.reclaim_task",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    if not raised(r) and not truthy(r):
        r=http_exc(409,f"cannot reclaim {g(i,'task_id','')}: not in a claimable state (not running, or unknown id)")
    x=bc_close(c,conn)
    if raised(r): return r
    return {"ok":True,"task_id":g(i,"task_id")}
def m_reassign_task(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    r=c.call("kanban_db.reassign_task",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    if not raised(r) and not truthy(r):
        r=http_exc(409,f"cannot reassign {g(i,'task_id','')}: unknown id, or still running (pass reclaim_first=true to release the claim first)")
    x=bc_close(c,conn)
    if raised(r): return r
    return {"ok":True,"task_id":g(i,"task_id"),"assignee":g(i,"profile")}

def m_get_config(c,i):
    cfg=c.call("_load_config_or_empty")
    if raised(cfg): return cfg
    k=g(g(cfg,"dashboard",{}),"kanban",{}) or {}
    dt=g(k,"default_tenant")
    return {"default_tenant":json.loads(json.dumps(dt)) if truthy(dt) else "",
            "lane_by_profile":truthy(g(k,"lane_by_profile",True)),
            "include_archived_by_default":truthy(g(k,"include_archived_by_default",False)),
            "render_markdown":truthy(g(k,"render_markdown",True))}
def m_run_estimate(c,i):
    if not strip(str(g(i,"title",""))):
        return {"ok":False,"reason":"a title is required to estimate"}
    m=c.call("importlib.import_module",module="agent.auxiliary_client")
    if raised(m): return {"ok":False,"reason":"auxiliary client unavailable"}
    aff=c.call("get_affinity_scope")
    resp=c.call("call_llm",title=g(i,"title"),body=g(i,"body"))
    if raised(resp):
        r={"ok":False,"reason":f"LLM error: {rtype(resp)}"}
        if truthy(aff): c.callvoid("reset_affinity_scope")
        return r
    parsed=g(resp,"parsed")
    o={}
    if not truthy(parsed):
        o={"ok":False,"reason":"could not parse an estimate from the model"}
    else:
        cx=g(parsed,"complexity",""); cu=strip(cx)
        valid=cu in ("S","M","L")
        rt=strip(str(g(parsed,"rationale","")))
        o={"ok":True,"est_tokens":g(parsed,"est_tokens"),
           "complexity":json.loads(json.dumps(g(parsed,"complexity"))) if valid else None,
           "rationale":rt if rt else None,"model":g(resp,"model")}
    if truthy(aff): c.callvoid("reset_affinity_scope")
    return o
def m_estimate_text(c,i):
    p=g(i,"payload",{}) or {}
    return c.call("_run_estimate",title=g(p,"title"),body=g(p,"body"),task_id=None)
def m_estimate_task(c,i):
    t=bc_core(c,i,"_require_task",g(i,"args"))
    if raised(t): return t
    return c.call("_run_estimate",title=g(t,"title"),body=g(t,"body"),task_id=g(i,"task_id"))
def m_decompose(c,i):
    p=g(i,"payload",{}) or {}
    return c.call("_run_aux",board=g(i,"board"),module="kanban_decompose",fn="decompose_task",task_id=g(i,"task_id"),author=g(p,"author"))
def m_specify(c,i):
    p=g(i,"payload",{}) or {}
    return c.call("_run_aux",board=g(i,"board"),module="kanban_specify",fn="specify_task",task_id=g(i,"task_id"),author=g(p,"author"))
def m_model_options(c,i):
    m=c.call("importlib.import_module",module="hermes_cli.inventory")
    if raised(m): return {"providers":[]}
    p=c.call("build_models_payload")
    if raised(p): return {"providers":[]}
    out=[]
    for r in (g(p,"providers",[]) or []):
        ms=g(r,"models")
        if not truthy(ms): continue
        slug=g(r,"slug"); label=g(r,"label")
        out.append({"slug":json.loads(json.dumps(slug)) if truthy(slug) else "",
                    "label":json.loads(json.dumps(label)) if truthy(label) else g(r,"slug"),
                    "models":json.loads(json.dumps(ms))})
    return {"providers":out}
def m_resolve_project(c,i):
    ref=str(g(i,"ref",""))
    if not strip(ref): return [None,None,None]
    m=c.call("importlib.import_module",module="hermes_cli.projects_db")
    if raised(m): return m
    p=c.call("pdb.get_project",ref=ref)
    if raised(p): return p
    if not truthy(p): return http_exc(400,g(i,"detail",""))
    pp=g(p,"primary_path")
    return [g(p,"id"),g(p,"name"),json.loads(json.dumps(pp)) if truthy(pp) else None]
def m_projects_by_id(c,i):
    m=c.call("importlib.import_module",module="hermes_cli.projects_db")
    if raised(m): return {}
    p=c.call("pdb.list_projects")
    if raised(p): return {}
    return {str(g(r,"id","")):json.loads(json.dumps(r)) for r in (p if isinstance(p,list) else [])}
def m_board_counts(c,i):
    ex=c.call("kanban_db.kanban_db_path",board=g(i,"slug"))
    if raised(ex) or not truthy(ex): return {}
    rows=c.call("conn.execute",sql="SELECT status, COUNT(*) AS n FROM tasks GROUP BY status")
    if raised(rows): return {}
    return {str(g(r,"status","")):g(r,"n",0) for r in (rows if isinstance(rows,list) else [])}
def m_default_workspace_kind(c,i):
    wd=strip(str(g(g(i,"board",{}),"default_workdir","")))
    if not wd: return "scratch"
    r=c.call("kbw._git_toplevel",path=wd)
    if raised(r): return "dir"
    return "worktree" if truthy(r) else "dir"
def m_annotate_board_meta(c,i):
    meta=g(i,"meta",{}) or {}
    w=c.call("_default_workspace_kind",board=json.loads(json.dumps(meta)))
    p=c.call("_resolve_project",ref=g(meta,"project_id"))
    o=json.loads(json.dumps(meta))
    if not raised(w): o["default_workspace_kind"]=w
    if not raised(p) and isinstance(p,list) and len(p)>1: o["project_name"]=json.loads(json.dumps(p[1]))
    return o

def m_bulk_update(c,i):
    ids=g(g(i,"payload",{}),"ids")
    if not truthy(ids): return http_exc(400,"ids is required")
    e,b,conn=bc_open(c,i)
    if e: return e
    r=c.call("_bulk_apply_one",conn=json.loads(json.dumps(conn)),ids=json.loads(json.dumps(ids)))
    x=bc_close(c,conn)
    if raised(r): return r
    return {"results":r}
def m_patch_status(c,i):
    return c.call("_apply_status",conn=g(i,"conn"),task_id=g(i,"task_id"))
def m_patch_title_body(c,i):
    e=c.callvoid("kanban_db.write_txn")
    if e: return e
    e=c.callvoid("conn.execute",sql="UPDATE tasks")
    if not e: e=c.callvoid("conn.execute",sql="INSERT INTO task_events")
    if e: return e
    return c.callvoid("kanban_db.notify_task_updated",task_id=g(i,"task_id"))
def m_set_status_direct(c,i):
    e=c.callvoid("kanban_db.write_txn")
    if e: return e
    prev=c.call("conn.execute",sql="SELECT status, current_run_id, worker_pid, claim_lock, worker_started_at FROM tasks WHERE id = ?")
    if raised(prev): return prev
    if not truthy(prev): return False
    r=c.call("conn.execute",sql="UPDATE tasks SET status")
    if raised(r): return r
    return c.call("kanban_db.recompute_ready")
def m_update_task(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    e=c.callvoid("_require_task",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    if not e: e=c.callvoid("_patch_status",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    if not e: e=c.callvoid("_patch_title_body",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    r=e if e else bc_core(c,i,"kanban_db.get_task",g(i,"args"))
    x=bc_close(c,conn)
    if x is not None and not raised(r): return x
    return r
def m_create_task(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    tid=c.call("kanban_db.create_task",conn=json.loads(json.dumps(conn)))
    if raised(tid):
        bc_close(c,conn); return tid
    task=c.call("kanban_db.get_task",conn=json.loads(json.dumps(conn)),task_id=json.loads(json.dumps(tid)))
    td=None
    if not raised(task) and truthy(task): td=c.call("_task_dict",task=task,board=json.loads(json.dumps(b)))
    x=bc_close(c,conn)
    return {"task":td}
def m_download_attachment(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    att=c.call("kanban_db.get_attachment",conn=json.loads(json.dumps(conn)),attachment_id=g(i,"attachment_id"))
    if raised(att):
        bc_close(c,conn); return att
    if not truthy(att):
        bc_close(c,conn); return http_exc(404,"attachment not found")
    chk=c.call("stored.relative_to")
    if raised(chk):
        bc_close(c,conn); return http_exc(404,"attachment file unavailable")
    isf=c.call("stored.is_file")
    if not truthy(isf):
        bc_close(c,conn); return http_exc(404,"attachment file missing on disk")
    x=bc_close(c,conn)
    return {"path":g(att,"stored_path"),"filename":g(att,"filename")}
def m_upload_task_attachment(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    e=c.callvoid("_require_task",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    if not e: e=c.callvoid("_safe_attachment_name")
    if not e: e=c.callvoid("kanban_db.task_attachments_dir")
    if not e: e=c.callvoid("_collision_free_path")
    if not e: e=c.callvoid("dest.write")
    if e:
        bc_close(c,conn); return e
    att=c.call("kanban_db.add_attachment",conn=json.loads(json.dumps(conn)),task_id=g(i,"task_id"))
    x=bc_close(c,conn)
    return {"attachment":None if raised(att) else json.loads(json.dumps(att))}
def m_list_diagnostics(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    d=c.call("_compute_task_diagnostics",conn=json.loads(json.dumps(conn)))
    x=bc_close(c,conn)
    if raised(d): return d
    return {"diagnostics":d,"count":0}
def m_list_active_workers(c,i): return bc_core(c,i,"conn.execute",g(i,"args"))
def m_compute_task_diagnostics(c,i):
    ids=g(i,"task_ids")
    if isinstance(ids,list) and len(ids)==0: return {}
    m=c.call("importlib.import_module",module="hermes_cli.config")
    if raised(m): return {}
    cfg=c.call("load_config")
    if raised(cfg): return {}
    rows=c.call("conn.execute",sql="SELECT * FROM tasks")
    if raised(rows): return {}
    o={}
    for r in (rows if isinstance(rows,list) else []):
        d=c.call("kd.compute_task_diagnostics",row=json.loads(json.dumps(r)))
        if not raised(d) and truthy(d): o[str(g(r,"id",""))]=d
    return o


def m_list_kanban_projects(c,i):
    m=c.call("importlib.import_module",module="hermes_cli.projects_db")
    if raised(m): return http_exc(500,"failed to list projects")
    p=c.call("pdb.list_projects")
    if raised(p): return http_exc(500,"failed to list projects")
    return {"projects":json.loads(json.dumps(p)) if isinstance(p,list) else []}
def m_list_boards(c,i):
    boards=c.call("kanban_db.list_boards")
    if raised(boards): return boards
    cur=c.call("kanban_db.get_current_board")
    if raised(cur): return cur
    pm=c.call("_projects_by_id")
    return {"boards":boards,"current":cur}
def m_validate_workdir(c,i):
    raw=str(g(i,"raw",""))
    if not raw.startswith("/"): return http_exc(400,"Project directory must be an absolute path.")
    d=c.call("Path.is_dir",path=raw)
    if raised(d): return d
    if not truthy(d): return http_exc(400,"Project directory must be an existing directory.")
    return raw
def m_switch_board(c,i):
    n=c.call("_existing_board_slug",slug=g(i,"slug"))
    if raised(n): return n
    e=c.callvoid("kanban_db.set_current_board",slug=json.loads(json.dumps(n)))
    if e: return e
    return {"current":n}
def m_delete_board(c,i):
    r=c.call("kanban_db.remove_board",slug=g(i,"slug"))
    if raised(r): return r
    cur=c.call("kanban_db.get_current_board")
    return {"result":r,"current":cur}
def m_create_board(c,i):
    e=c.callvoid("_validate_workdir")
    if e: return e
    p=c.call("_resolve_project",ref=g(g(i,"payload",{}),"project_id"))
    if raised(p): return p
    m=c.call("kanban_db.create_board")
    if raised(m): return m
    ann=c.call("_annotate_board_meta",meta=json.loads(json.dumps(m)))
    cur=c.call("kanban_db.get_current_board")
    return {"board":ann,"current":cur}
def m_rename_board(c,i):
    n=c.call("_existing_board_slug",slug=g(i,"slug"))
    if raised(n): return n
    m=c.call("kanban_db.write_board_metadata",slug=json.loads(json.dumps(n)))
    if raised(m): return m
    ann=c.call("_annotate_board_meta",meta=json.loads(json.dumps(m)))
    return {"board":ann}
def m_list_profile_roster(c,i):
    m=c.call("importlib.import_module",module="hermes_cli.profiles")
    if raised(m): return {"profiles":[]}
    p=c.call("profiles_mod.list_profiles")
    if raised(p): return {"profiles":[]}
    return {"profiles":p}
def m_update_profile_description(c,i):
    m=c.call("importlib.import_module",module="hermes_cli.profiles")
    if raised(m): return http_exc(500,"failed to update profile")
    canon=c.call("profiles_mod.normalize_profile_name",name=g(i,"profile_name"))
    if raised(canon): return canon
    d=c.call("profile_dir.is_dir")
    if raised(d): return http_exc(500,"failed to update profile")
    if not truthy(d): return http_exc(404,f"profile '{g(i,'profile_name','')}' not found")
    e=c.callvoid("profiles_mod.write_profile_meta")
    if e: return e
    return {"ok":True,"profile":canon,"description":g(g(i,"payload",{}),"description")}
def m_auto_describe_profile(c,i):
    m=c.call("importlib.import_module",module="hermes_cli.profile_describer")
    if raised(m): return http_exc(500,"describer crashed")
    out=c.call("profile_describer.describe_profile",name=g(i,"profile_name"))
    if raised(out): return out
    return {"ok":truthy(g(out,"ok")),"profile":g(out,"profile_name"),"reason":g(out,"reason"),"description":g(out,"description")}
def m_validated_profile_name(c,i):
    n=strip(str(g(i,"raw","")))
    if n and truthy(g(i,"profiles_mod")):
        ex=c.call("profiles_mod.profile_exists",name=n)
        if not raised(ex) and not truthy(ex): return http_exc(400,f"profile '{n}' does not exist")
    return n
def m_get_orchestration_settings(c,i):
    cfg=c.call("_load_config_or_empty")
    if raised(cfg): return cfg
    p=c.call("profiles_mod.get_active_profile_name")
    active="default" if raised(p) or not truthy(p) else str(p)
    return {"orchestrator_profile":"","default_assignee":"","auto_decompose":True,
            "auto_promote_children":True,"resolved_orchestrator_profile":active,
            "resolved_default_assignee":active,"active_profile":active}
def m_set_orchestration_settings(c,i):
    m=c.call("importlib.import_module",module="hermes_cli.config")
    if raised(m): return http_exc(500,"failed to load config")
    cfg=c.call("load_config")
    if raised(cfg): return http_exc(500,"failed to load config")
    e=c.callvoid("save_config",cfg=json.loads(json.dumps(cfg)))
    if e: return http_exc(500,"failed to save config")
    return m_get_orchestration_settings(c,i)
def m_get_workflow(c,i): return c.call("kanban_workflow.DEFAULT_WORKFLOW.to_dict")
def m_get_board(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    tasks=c.call("kanban_db.list_tasks",conn=json.loads(json.dumps(conn)))
    if raised(tasks):
        bc_close(c,conn); return tasks
    diags=c.call("_compute_task_diagnostics",conn=json.loads(json.dumps(conn)))
    sums=c.call("kanban_db.latest_summaries",conn=json.loads(json.dumps(conn)))
    rs=c.call("kanban_db.current_run_started_ats",conn=json.loads(json.dumps(conn)))
    tenants=c.call("conn.execute",sql="SELECT DISTINCT tenant FROM tasks")
    asg=c.call("kanban_db.known_assignees",conn=json.loads(json.dumps(conn)))
    x=bc_close(c,conn)
    return {"columns":{}}
def m_get_board_endpoint(c,i):
    b=c.call("kanban_db.get_current_board")
    if raised(b): return b
    return c.call("_read_board",board=b,tenant=g(i,"tenant"))
def m_stream_events(c,i):
    ok=c.call("_ws_upgrade_authorized")
    if raised(ok): return ok
    if not truthy(ok): return c.callvoid("ws.close")
    e=c.callvoid("ws.accept")
    if e: return e
    raw=c.call("_ws_board",board=g(i,"board"))
    if raised(raw): return raw
    board=c.call("_ws_board_live",board=json.loads(json.dumps(raw)))
    if raised(board): return board
    since=c.call("_since_param")
    if raised(since): return since
    return c.callvoid("tail.shutdown")
def m_et_latest(c,i): return c.call("tail._latest",board=g(i,"board"))
def m_et_fetch(c,i): return c.call("tail._fetch",cursor=g(i,"cursor"))
def m_et_close(c,i): return c.callvoid("tail._close")
def m_et_run(c,i): return c.call("tail._run",cursor=g(i,"cursor"))
def m_et_poll(c,i): return c.call("tail.poll",cursor=g(i,"cursor"))
def m_et_shutdown(c,i): return c.callvoid("tail.shutdown")
def m_run_transfer(c,i):
    r=c.call("run_in_executor",fn=g(i,"fn"))
    if not raised(r): return r
    t=rtype(r)
    if exc_is(t,"FileNotFoundError"): return http_exc(404,rmsg(r))
    if exc_is(t,"ValueError"): return http_exc(400,rmsg(r))
    return http_exc(500,rmsg(r))
def m_export_board(c,i):
    m=c.call("importlib.import_module",module="hermes_cli.kanban_transfer")
    if raised(m): return m
    return c.call("_run_transfer",fn="export_board",slug=g(i,"slug"))
def m_import_board(c,i):
    m=c.call("importlib.import_module",module="hermes_cli.kanban_transfer")
    if raised(m): return m
    a=strip(str(g(g(i,"body",{}),"archive","")))
    if not a: return http_exc(400,"archive path is required")
    r=c.call("_run_transfer",fn="import_board")
    if raised(r): return r
    cur=c.call("kanban_db.get_current_board")
    o=json.loads(json.dumps(r)); o["current"]=cur; return o

def m_bulk_apply_one(c,i):
    e={"id":g(i,"tid"),"ok":True}
    r=c.call("_apply_status",conn=g(i,"conn"),task_id=g(i,"tid"))
    if not truthy(r) and not raised(r):
        e["ok"]=False
        why=c.call("_open_parent_refusal",conn=g(i,"conn"),task_id=g(i,"tid"))
        e["error"]="transition refused" if raised(why) else json.loads(json.dumps(why))
    return e
def m_inspect_run_endpoint(c,i):
    e,b,conn=bc_open(c,i)
    if e: return e
    r=c.call("_require_run",conn=json.loads(json.dumps(conn)),run_id=g(i,"run_id"))
    x=bc_close(c,conn)
    if raised(r): return r
    o={"run_id":g(i,"run_id")}
    if truthy(g(r,"ended_at")): o.update(alive=False,reason="run already ended"); return o
    if not truthy(g(r,"worker_pid")): o.update(alive=False,reason="no worker_pid recorded"); return o
    info=c.call("_psutil.Process",pid=g(r,"worker_pid"))
    if raised(info):
        o.update(alive=False,reason=("process not found" if exc_is(rtype(info),"FileNotFoundError") else "process unavailable"))
        return o
    o.update(alive=True,pid=g(r,"worker_pid"),cpu_percent=g(info,"cpu_percent"),status=g(info,"status"))
    return o

MIRRORS = {
 "_board_conn":m_board_conn,"_with_board_pinned":m_with_board_pinned,"_run_aux":m_run_aux,
 "_map_errors":m_map_errors,"_errors_to_500":m_errors_to_500,"_parents_blocking_ready":m_parents_blocking_ready,
 "_configured_home_channels":m_configured_home_channels,"get_task":m_get_task,
 "list_task_attachments":m_list_task_attachments,"remove_attachment":m_remove_attachment,
 "delete_task":m_delete_task,"add_link":m_add_link,"delete_link":m_delete_link,"add_comment":m_add_comment,
 "get_stats":m_get_stats,"get_assignees":m_get_assignees,"get_run_endpoint":m_get_run_endpoint,
 "get_home_channels":m_get_home_channels,"_home_for_platform":m_home_for_platform,
 "subscribe_home":m_subscribe_home,"unsubscribe_home":m_unsubscribe_home,"get_task_log":m_get_task_log,
 "dispatch":m_dispatch,"terminate_run_endpoint":m_terminate_run,"reclaim_task_endpoint":m_reclaim_task,
 "reassign_task_endpoint":m_reassign_task,"get_config":m_get_config,"_run_estimate":m_run_estimate,
 "estimate_text_endpoint":m_estimate_text,"estimate_task_endpoint":m_estimate_task,
 "decompose_task_endpoint":m_decompose,"specify_task_endpoint":m_specify,"model_options":m_model_options,
 "_resolve_project":m_resolve_project,"_projects_by_id":m_projects_by_id,"_board_counts":m_board_counts,
 "_default_workspace_kind":m_default_workspace_kind,"_annotate_board_meta":m_annotate_board_meta,
 "bulk_update":m_bulk_update,"_patch_status":m_patch_status,"_patch_title_body":m_patch_title_body,
 "_set_status_direct":m_set_status_direct,"update_task":m_update_task,"create_task":m_create_task,
 "download_attachment":m_download_attachment,"upload_task_attachment":m_upload_task_attachment,
 "_bulk_apply_one":m_bulk_apply_one,"inspect_run_endpoint":m_inspect_run_endpoint,
 "list_diagnostics":m_list_diagnostics,"list_active_workers":m_list_active_workers,
 "_compute_task_diagnostics":m_compute_task_diagnostics,"list_kanban_projects":m_list_kanban_projects,
 "list_boards":m_list_boards,"_validate_workdir":m_validate_workdir,"switch_board":m_switch_board,
 "delete_board":m_delete_board,"create_board_endpoint":m_create_board,"rename_board":m_rename_board,
 "list_profile_roster":m_list_profile_roster,"update_profile_description":m_update_profile_description,
 "auto_describe_profile":m_auto_describe_profile,"_validated_profile_name":m_validated_profile_name,
 "get_orchestration_settings":m_get_orchestration_settings,"set_orchestration_settings":m_set_orchestration_settings,
 "get_workflow":m_get_workflow,"get_board":m_get_board,"get_board_endpoint":m_get_board_endpoint,
 "stream_events":m_stream_events,"_latest":m_et_latest,"_fetch":m_et_fetch,"_close":m_et_close,
 "_run":m_et_run,"latest":m_et_latest,"poll":m_et_poll,"shutdown":m_et_shutdown,
 "_run_transfer":m_run_transfer,"export_board_endpoint":m_export_board,"import_board_endpoint":m_import_board,
}

def canon(v):
    if isinstance(v,dict): return {k:canon(v[k]) for k in sorted(v)}
    if isinstance(v,list): return [canon(x) for x in v]
    if isinstance(v,float) and not (v==v) or isinstance(v,float) and v in (float("inf"),float("-inf")): return None
    if isinstance(v,float) and v==int(v): return int(v)
    return v

def main():
    total=mis=0
    for line in sys.stdin:
        line=line.strip()
        if not line.startswith("{"): continue
        rec=json.loads(line); func=rec.get("func",""); i=rec.get("in",{}) or {}
        out=rec.get("out",{}) or {}
        c=Ctx(i)
        try:
            rv=MIRRORS[func](c,i)
        except KeyError:
            rv={"error":"unknown func"}
        exp={"ret":rv if rv is not None else None,"trace":c.trace,"state":{}}
        got={"ret":out.get("ret"),"trace":out.get("trace"),"state":out.get("state",{})}
        total+=1
        if canon(exp)!=canon(got):
            mis+=1
            if mis<=12:
                print("MISMATCH",func,json.dumps({"exp":canon(exp),"got":canon(got)},ensure_ascii=False)[:1200])
    if total==0:
        print("ORACLE kanban_plugin_api_flow2: 0 cases, 1 mismatches"); return 2
    print(f"ORACLE kanban_plugin_api_flow2: {total} cases, {mis} mismatches")
    return 0 if mis==0 else 1

if __name__=="__main__":
    sys.exit(main())
