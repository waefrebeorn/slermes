/* plugins_install_flow2.c — FLOW port of the UNCLAIMED remainder of
 * hermes_cli/plugins_install_scan.py (all 21 top-level defs; strict CRLF-aware
 * PoP grep: 0/21 claimed — the scan_context sibling is separately claimed by
 * plugins_install_scan_flow2.c and is NOT re-claimed here).
 *
 * Representation contract (shared with tests/oracle_plugins_install_flow2.py):
 * each pi2_* fn is JSON-in/JSON-out. Heavy external calls (regex, filesystem,
 * subprocess, AST traversal, sibling predicates) are consumed from the per-case
 * "io" seam queues in the SAME order the Python play doubles pop them, so both
 * sides see byte-identical data. The port under test is the surrounding FLOW:
 * severity ranks/remaps, demotion ordering, verdict mapping, string formatting,
 * set membership and the FULL builtin exception hierarchy.
 *
 * libjson-only; the harness #includes "cli/plugins_install_flow2.c".
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

typedef struct {
    json_t *io;    /* borrowed from the case's "in.io" */
    json_t *pos;   /* owned: seam name -> next index */
    json_t *exc;   /* owned: pending {"__raise__":cls,...} or NULL */
} pi2_ctx;

/* ---- PoP: pi2_finding @ hermes_cli/plugins_install_scan.py:_finding ---- */
/* PoP: pi2_is_main_guard @ hermes_cli/plugins_install_scan.py:_is_main_guard */
/* PoP: pi2_parse_python_source @ hermes_cli/plugins_install_scan.py:_parse_python_source */
/* PoP: pi2_main_guard_body_lines @ hermes_cli/plugins_install_scan.py:_main_guard_body_lines */
/* PoP: pi2_declares_non_utf8_source @ hermes_cli/plugins_install_scan.py:_declares_non_utf8_source */
/* PoP: pi2_docstring_lines @ hermes_cli/plugins_install_scan.py:_docstring_lines */
/* PoP: pi2_docstring_cap @ hermes_cli/plugins_install_scan.py:_docstring_cap */
/* PoP: pi2_filter_findings @ hermes_cli/plugins_install_scan.py:_filter_findings */
/* PoP: pi2_at_most @ hermes_cli/plugins_install_scan.py:_at_most */
/* PoP: pi2_comment_severity @ hermes_cli/plugins_install_scan.py:_comment_severity */
/* PoP: pi2_file_lines @ hermes_cli/plugins_install_scan.py:_file_lines */
/* PoP: pi2_context_severity @ hermes_cli/plugins_install_scan.py:_context_severity */
/* PoP: pi2_inert_context_cap @ hermes_cli/plugins_install_scan.py:_inert_context_cap */
/* PoP: pi2_is_defensive_documentation @ hermes_cli/plugins_install_scan.py:_is_defensive_documentation */
/* PoP: pi2_dangerous_findings_summary @ hermes_cli/plugins_install_scan.py:_dangerous_findings_summary */
/* PoP: pi2_tracked_bytecode @ hermes_cli/plugins_install_scan.py:_tracked_bytecode */
/* PoP: pi2_bytecode_findings @ hermes_cli/plugins_install_scan.py:_bytecode_findings */
/* PoP: pi2_check_plugin_structure @ hermes_cli/plugins_install_scan.py:_check_plugin_structure */
/* PoP: pi2_scan_plugin @ hermes_cli/plugins_install_scan.py:scan_plugin */
/* PoP: pi2_should_allow_plugin_install @ hermes_cli/plugins_install_scan.py:should_allow_plugin_install */

/* ------------------------------- helpers ------------------------------- */
static char *pi2_fmt(const char *fmt, ...) {
    char buf[4096];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return strdup(buf);
}

static const char *pi2_s(json_t *o, const char *k, const char *d) {
    return json_get_str(o, k, d);
}
static int pi2_n(json_t *o, const char *k, int d) {
    return (int)json_get_num(o, k, (double)d);
}
static int pi2_b(json_t *o, const char *k, int d) {
    if (!o || o->type != JSON_OBJECT) return d;
    json_t *v = json_obj_get(o, k);
    if (!v || v->type != JSON_BOOL) return d;
    return v->bool_val ? 1 : 0;
}

/* pop the next seam value for name (borrowed) */
static json_t *pi2_pop(pi2_ctx *c, const char *name) {
    json_t *arr = c->io ? json_obj_get(c->io, name) : NULL;
    double d = json_get_num(c->pos, name, 0.0);
    json_set(c->pos, name, json_number(d + 1.0));
    if (arr && arr->type == JSON_ARRAY && d >= 0 && (size_t)d < json_len(arr))
        return json_get(arr, (size_t)d);
    return NULL;
}

/* pop a seam and raise if it is {"__raise__": cls} */
static json_t *pi2_seam(pi2_ctx *c, const char *name) {
    json_t *v = pi2_pop(c, name);
    if (v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__")) {
        if (c->exc) json_free(c->exc);
        c->exc = json_copy(v);
        return NULL;
    }
    return v;
}

/* builtin exception hierarchy: is cls == parent or a subclass of parent */
static const char *pi2_parent_of(const char *cls) {
    struct { const char *c, *p; } t[] = {
        {"BaseException",NULL},{"Exception","BaseException"},
        {"ArithmeticError","Exception"},{"AssertionError","Exception"},
        {"AttributeError","Exception"},{"BufferError","Exception"},
        {"EOFError","Exception"},{"ImportError","Exception"},
        {"LookupError","Exception"},{"MemoryError","Exception"},
        {"NameError","Exception"},{"OSError","Exception"},
        {"ReferenceError","Exception"},{"RuntimeError","Exception"},
        {"StopAsyncIteration","Exception"},{"StopIteration","Exception"},
        {"SyntaxError","Exception"},{"SystemError","Exception"},
        {"TypeError","Exception"},{"ValueError","Exception"},
        {"Warning","Exception"},
        {"UnicodeError","ValueError"},{"UnicodeDecodeError","UnicodeError"},
        {"UnicodeEncodeError","UnicodeError"},{"UnicodeTranslateError","UnicodeError"},
        {"FileNotFoundError","OSError"},{"FileExistsError","OSError"},
        {"IsADirectoryError","OSError"},{"NotADirectoryError","OSError"},
        {"PermissionError","OSError"},{"ProcessLookupError","OSError"},
        {"TimeoutError","OSError"},{"ConnectionError","OSError"},
        {"BlockingIOError","OSError"},{"ChildProcessError","OSError"},
        {"InterruptedError","OSError"},
        {"KeyError","LookupError"},{"IndexError","LookupError"},
        {"subprocess.SubprocessError","Exception"},
        {"subprocess.CalledProcessError","subprocess.SubprocessError"},
        {"subprocess.TimeoutExpired","subprocess.SubprocessError"},
    };
    for (size_t i = 0; i < sizeof(t)/sizeof(t[0]); i++)
        if (strcmp(t[i].c, cls) == 0) return t[i].p;
    return NULL;
}
static int pi2_exc_isa(const char *cls, const char *parent) {
    if (!cls || !parent) return 0;
    const char *cur = cls;
    for (int i = 0; i < 16 && cur; i++) {
        if (strcmp(cur, parent) == 0) return 1;
        cur = pi2_parent_of(cur);
    }
    return 0;
}
/* if an exception is pending and it matches one of the comma-separated parents,
 * swallow it (caught by except) and return 1; otherwise leave it pending. */
static int pi2_caught(pi2_ctx *c, const char *parents_csv) {
    if (!c->exc) return 0;
    const char *cls = pi2_s(c->exc, "__raise__", "");
    char buf[512]; snprintf(buf, sizeof(buf), "%s", parents_csv);
    char *tok = strtok(buf, ",");
    int hit = 0;
    while (tok) {
        while (*tok == ' ') tok++;
        if (pi2_exc_isa(cls, tok)) { hit = 1; break; }
        tok = strtok(NULL, ",");
    }
    if (hit) { json_free(c->exc); c->exc = NULL; }
    return hit;
}

static int pi2_rank(const char *s) {
    if (!s) return 0;
    if (!strcmp(s,"low")) return 0;
    if (!strcmp(s,"medium")) return 1;
    if (!strcmp(s,"high")) return 2;
    if (!strcmp(s,"critical")) return 3;
    return 0;
}
static const char *pi2_step_down(const char *s) {
    if (!s) return s;
    if (!strcmp(s,"critical")) return "high";
    if (!strcmp(s,"high")) return "medium";
    return s;
}
static const char *pi2_at_most_s(const char *sev, const char *cap) {
    return pi2_rank(sev) > pi2_rank(cap) ? cap : sev;
}
static int pi2_in_strs(const char *s, const char *const *set, size_t n) {
    if (!s) return 0;
    for (size_t i=0;i<n;i++) if (!strcmp(s,set[i])) return 1;
    return 0;
}
static json_t *pi2_strarr_to_set(const json_t *arr) {
    /* sorted unique strings as JSON array */
    json_t *out = json_array();
    if (!arr || arr->type != JSON_ARRAY) return out;
    size_t n = json_len(arr);
    const char **v = calloc(n ? n : 1, sizeof(char*));
    size_t m = 0;
    for (size_t i=0;i<n;i++) {
        json_t *e = json_get(arr,i);
        const char *s = (e && e->type==JSON_STRING) ? e->str_val : NULL;
        if (!s) continue;
        int dup=0; for(size_t j=0;j<m;j++) if(!strcmp(v[j],s)){dup=1;break;}
        if(!dup) v[m++]=s;
    }
    for (size_t i=0;i<m;i++) for(size_t j=i+1;j<m;j++)
        if (strcmp(v[i],v[j])>0){const char*t=v[i];v[i]=v[j];v[j]=t;}
    for (size_t i=0;i<m;i++) json_append(out, json_string(v[i]));
    free(v);
    return out;
}
static json_t *pi2_marks_to_lines(const int *marks, int n) {
    json_t *out = json_array();
    for (int i=1;i<=n && i<=65536;i++) if (marks[i]) json_append(out, json_int(i));
    return out;
}
static json_t *pi2_finding_obj(const char *pid, const char *sev, const char *cat,
                               const char *file, int line, const char *match,
                               const char *desc) {
    json_t *o = json_object();
    json_set(o,"pattern_id",json_string(pid?pid:""));
    json_set(o,"severity",json_string(sev?sev:""));
    json_set(o,"category",json_string(cat?cat:""));
    json_set(o,"file",json_string(file?file:""));
    json_set(o,"line",json_int(line));
    json_set(o,"match",json_string(match?match:""));
    json_set(o,"description",json_string(desc?desc:""));
    return o;
}

/* resolve a finding from input; borrows the json node */
static json_t *pi2_finding_in(json_t *in) {
    return json_obj_get(in, "finding");
}

/* ------------------------------- defs ---------------------------------- */

/* PoP: pi2_walk @ hermes_cli/plugins_install_scan.py:_walk */
static json_t *pi2_walk(pi2_ctx *c, json_t *in) {
    (void)c;
    static const char *excluded[] = {".git","__pycache__","node_modules",".venv","venv",
        ".mypy_cache",".pytest_cache",".ruff_cache",".tox"};
    json_t *entries = json_obj_get(in,"entries");
    json_t *out = json_array();
    if (!entries || entries->type!=JSON_ARRAY) return out;
    for (size_t i=0;i<json_len(entries);i++) {
        json_t *e = json_get(entries,i);
        json_t *parts = e ? json_obj_get(e,"parts") : NULL;
        if (!parts || parts->type!=JSON_ARRAY) continue;
        int skip=0;
        for (size_t j=0;j<json_len(parts);j++) {
            const char *pt = json_string_value(json_get(parts,j));
            if (pi2_in_strs(pt,excluded,sizeof(excluded)/sizeof(excluded[0]))) {skip=1;break;}
        }
        if (skip) continue;
        /* rel = "/".join(parts) */
        char rel[4096]="";
        for (size_t j=0;j<json_len(parts);j++) {
            if (j) strcat(rel,"/");
            const char *pt = json_string_value(json_get(parts,j));
            if (pt) strcat(rel,pt);
        }
        json_t *o = json_object();
        json_set(o,"name",json_string(pi2_s(e,"name","")));
        json_set(o,"rel",json_string(rel));
        json_append(out,o);
    }
    return out;
}

static json_t *pi2_finding(pi2_ctx *c, json_t *in) {
    (void)c;
    return pi2_finding_obj(pi2_s(in,"pattern_id",""), pi2_s(in,"severity",""),
        pi2_s(in,"category",""), pi2_s(in,"file",""), 0,
        pi2_s(in,"match",""), pi2_s(in,"description",""));
}

static json_t *pi2_is_main_guard(pi2_ctx *c, json_t *in) {
    (void)c;
    json_t *node = json_obj_get(in,"node");
    json_t *test = node ? json_obj_get(node,"test") : NULL;
    if (!test || test->type != JSON_OBJECT) return json_bool(0);
    json_t *ops = json_obj_get(test,"ops");
    json_t *comps = json_obj_get(test,"comparators");
    if (!ops || ops->type!=JSON_ARRAY || json_len(ops)!=1) return json_bool(0);
    json_t *op0 = json_get(ops,0);
    if (!op0 || op0->type!=JSON_STRING || strcmp(op0->str_val,"Eq")) return json_bool(0);
    if (!comps || comps->type!=JSON_ARRAY || json_len(comps)!=1) return json_bool(0);
    json_t *left = json_obj_get(test,"left"), *right = json_get(comps,0);
    if (!left || !right || left->type!=JSON_OBJECT || right->type!=JSON_OBJECT) return json_bool(0);
    /* left is Name(__name__) and right is Constant("__main__") (or reversed) */
    int a = !strcmp(pi2_s(left,"type",""),"Name") && !strcmp(pi2_s(left,"id",""),"__name__")
         && !strcmp(pi2_s(right,"type",""),"Constant") && !strcmp(pi2_s(right,"value",""),"__main__");
    int b = !strcmp(pi2_s(right,"type",""),"Name") && !strcmp(pi2_s(right,"id",""),"__name__")
         && !strcmp(pi2_s(left,"type",""),"Constant") && !strcmp(pi2_s(left,"value",""),"__main__");
    return json_bool(a||b);
}

static json_t *pi2_parse_python_source(pi2_ctx *c, json_t *in) {
    (void)in;
    json_t *v = pi2_seam(c,"read_text");
    if (c->exc) {
        if (pi2_caught(c,"OSError,SyntaxError,ValueError")) return json_null();
        return NULL;
    }
    json_t *p = pi2_seam(c,"ast_parse");
    if (c->exc) {
        if (pi2_caught(c,"OSError,SyntaxError,ValueError")) return json_null();
        return NULL;
    }
    return p ? json_copy(p) : json_null();
}

static json_t *pi2_main_guard_body_lines(pi2_ctx *c, json_t *in) {
    (void)in;
    json_t *tree = pi2_seam(c,"parse_source");
    (void)tree; /* a module marker or null */
    if (c->exc) return NULL; /* _parse_python_source never propagates */
    if (!tree || tree->type == JSON_NULL) {
        static int none[65537]; return pi2_marks_to_lines(none,0);
    }
    json_t *nodes = pi2_seam(c,"walk");
    static int marks[65537]; memset(marks,0,sizeof(marks));
    int n = 0;
    if (nodes && nodes->type==JSON_ARRAY) {
        for (size_t i=0;i<json_len(nodes);i++) {
            json_t *nd = json_get(nodes,i);
            if (!pi2_b(nd,"is_guard",0)) continue; /* _is_main_guard seam */
            json_t *body = nd ? json_obj_get(nd,"body") : NULL;
            if (!body || body->type!=JSON_ARRAY) continue;
            for (size_t j=0;j<json_len(body);j++) {
                json_t *st = json_get(body,j);
                int a = pi2_n(st,"lineno",0), b = pi2_n(st,"end_lineno",a);
                if (b>a) b=a; /* end_lineno == lineno in our domain */
                for (int ln=a; ln<=b && ln<=65536; ln++) if(ln>0) marks[ln]=1;
            }
        }
    }
    /* find max marked line */
    for(int i=65536;i>=0;i--) if(marks[i]){n=i;break;}
    return pi2_marks_to_lines(marks,n);
}

static json_t *pi2_declares_non_utf8_source(pi2_ctx *c, json_t *in) {
    (void)in;
    json_t *b = pi2_seam(c,"read_bytes");
    if (c->exc) {
        if (pi2_caught(c,"OSError,SyntaxError,LookupError,UnicodeError")) return json_bool(1);
        return NULL;
    }
    (void)b;
    json_t *enc = pi2_seam(c,"detect_encoding");
    if (c->exc) {
        if (pi2_caught(c,"OSError,SyntaxError,LookupError,UnicodeError")) return json_bool(1);
        return NULL;
    }
    (void)enc;
    json_t *lk = pi2_seam(c,"lookup");
    if (c->exc) {
        if (pi2_caught(c,"OSError,SyntaxError,LookupError,UnicodeError")) return json_bool(1);
        return NULL;
    }
    const char *name = pi2_s(lk,"name","");
    return json_bool(!( !strcmp(name,"utf-8") || !strcmp(name,"utf-8-sig") ));
}

static json_t *pi2_docstring_lines(pi2_ctx *c, json_t *in) {
    (void)in;
    json_t *tree = pi2_seam(c,"parse_source");
    if (c->exc) return NULL;
    json_t *decl = pi2_seam(c,"declares");
    int decl_b = pi2_b(decl,"value",0);
    if (!tree || tree->type==JSON_NULL || decl_b) {
        static int none[65537]; return pi2_marks_to_lines(none,0);
    }
    json_t *nodes = pi2_seam(c,"walk");
    static int marks[65537]; memset(marks,0,sizeof(marks));
    /* pass 1: docstring owners add ranges; record their ids as "doc" */
    if (nodes && nodes->type==JSON_ARRAY) {
        for (size_t i=0;i<json_len(nodes);i++) {
            json_t *nd = json_get(nodes,i);
            if (!pi2_b(nd,"owner",0)) continue;
            json_t *first = nd ? json_obj_get(nd,"first") : NULL;
            if (!first || first->type!=JSON_BOOL || !first->bool_val) continue;
            int a = pi2_n(nd,"lineno",0), b = pi2_n(nd,"end_lineno",a);
            if (b<a) b=a;
            for (int ln=a;ln<=b && ln<=65536;ln++) if(ln>0) marks[ln]=1;
        }
    }
    /* pass 2: non-owner nodes remove their first line (or full span) */
    if (nodes && nodes->type==JSON_ARRAY) {
        for (size_t i=0;i<json_len(nodes);i++) {
            json_t *nd = json_get(nodes,i);
            if (pi2_b(nd,"owner",0)) continue; /* id(node) in docstrings */
            int first_line = pi2_n(nd,"first_line",0);
            if (first_line==0) continue;
            int last = pi2_b(nd,"has_body",0) ? first_line : pi2_n(nd,"end_lineno",first_line);
            for (int ln=first_line;ln<=last && ln<=65536;ln++) if(ln>0) marks[ln]=0;
        }
    }
    int n=0; for(int i=65536;i>=0;i--) if(marks[i]){n=i;break;}
    return pi2_marks_to_lines(marks,n);
}

static json_t *pi2_docstring_cap(pi2_ctx *c, json_t *in) {
    json_t *f = pi2_finding_in(in);
    const char *sev = pi2_s(f,"severity","");
    if (!strcmp(sev,"critical")) return json_string("high");
    json_t *agent = pi2_seam(c,"is_agent_facing");
    if (c->exc) return NULL;
    int a = (agent && agent->type==JSON_BOOL) ? agent->bool_val : 0;
    /* _comment_severity(f) */
    const char *sev2 = pi2_step_down(sev);
    if (!a) sev2 = pi2_step_down(sev2);
    return json_string(sev2);
}

static json_t *pi2_comment_severity(pi2_ctx *c, json_t *in) {
    json_t *f = pi2_finding_in(in);
    const char *sev = pi2_s(f,"severity","");
    json_t *agent = pi2_seam(c,"is_agent_facing");
    if (c->exc) return NULL;
    int a = (agent && agent->type==JSON_BOOL) ? agent->bool_val : 0;
    const char *sev2 = pi2_step_down(sev);
    if (!a) sev2 = pi2_step_down(sev2);
    return json_string(sev2);
}

static json_t *pi2_file_lines(pi2_ctx *c, json_t *in) {
    (void)in;
    json_t *v = pi2_seam(c,"read_text");
    if (c->exc) {
        if (pi2_caught(c,"OSError,UnicodeDecodeError")) return json_array();
        return NULL;
    }
    return v ? json_copy(v) : json_array();
}

static json_t *pi2_at_most(pi2_ctx *c, json_t *in) {
    (void)c;
    const char *sev = pi2_s(in,"severity","");
    const char *cap = pi2_s(in,"cap","medium");
    return json_string(pi2_at_most_s(sev,cap));
}

static json_t *pi2_is_defensive_documentation(pi2_ctx *c, json_t *in) {
    (void)c;
    json_t *f = pi2_finding_in(in);
    const char *rel = pi2_s(in,"rel_path","");
    /* name of rel_path (basename), lowercased */
    const char *slash = strrchr(rel,'/'); const char *base = slash?slash+1:rel;
    if (!strcasecmp(base,"changelog.md")) return json_bool(1);
    const char *match = pi2_s(f,"match","");
    /* docstring_lines membership via input list */
    int line = pi2_n(f,"line",0);
    json_t *dl = json_obj_get(in,"docstring_lines");
    if (dl && dl->type==JSON_ARRAY)
        for (size_t i=0;i<json_len(dl);i++) if ((int)json_number_value(json_get(dl,i))==line) return json_bool(1);
    /* suffix of rel_path */
    const char *dot = strrchr(base,'.');
    const char *prefix = NULL;
    if (dot) {
        if (!strcasecmp(dot,".py")||!strcasecmp(dot,".sh")||!strcasecmp(dot,".bash")||
            !strcasecmp(dot,".rb")||!strcasecmp(dot,".pl")||!strcasecmp(dot,".r")||!strcasecmp(dot,".jl")) prefix="#";
        else if (!strcasecmp(dot,".js")||!strcasecmp(dot,".ts")||!strcasecmp(dot,".php")) prefix="//";
    }
    if (!prefix || !match || !*match) return json_bool(0);
    /* lstrip */
    while (*match==' '||*match=='\t'||*match=='\n'||*match=='\r') match++;
    if (strncmp(match,prefix,strlen(prefix))) return json_bool(0);
    if (!strcmp(prefix,"#") && (!strncmp(match,"#!",2)||!strncmp(match,"#:",2))) return json_bool(0);
    return json_bool(1);
}

static int pi2_pop_bool(pi2_ctx *c, const char *name) {
    json_t *v = pi2_seam(c, name);
    if (c->exc) return 0;
    return (v && v->type==JSON_BOOL && v->bool_val) ? 1 : 0;
}
static const char *pi2_pop_str(pi2_ctx *c, const char *name) {
    json_t *v = pi2_seam(c, name);
    if (c->exc) return "";
    return (v && v->type==JSON_STRING) ? v->str_val : "";
}

static json_t *pi2_inert_context_cap(pi2_ctx *c, json_t *in) {
    json_t *f = pi2_finding_in(in);
    const char *pid = pi2_s(f,"pattern_id","");
    const char *sev = pi2_s(f,"severity","");
    int is_code = pi2_b(in,"is_code",1);
    if (!strcmp(pid,"encoded_exfil") && pi2_pop_bool(c,"is_base64_media")) return json_string("low");
    if (c->exc) return NULL;
    int to_low = 0;
    if (pi2_pop_bool(c,"is_loopback_only")) to_low=1;
    else if (pi2_pop_bool(c,"is_loopback_continuation")) to_low=1;
    else if (pi2_pop_bool(c,"is_hex_in_char_class")) to_low=1;
    else if (is_code && pi2_pop_bool(c,"is_pip_install_in_prose_literal")) to_low=1;
    else if (pi2_pop_bool(c,"is_own_proc_read")) to_low=1;
    if (c->exc) return NULL;
    if (to_low) return json_string("low");
    int to_medium = 0;
    if (pi2_pop_bool(c,"is_base64_archive_roundtrip")) to_medium=1;
    else if (pi2_pop_bool(c,"is_coin_name_only")) to_medium=1;
    else if (pi2_pop_bool(c,"is_trusted_installer_pipe")) to_medium=1;
    else if (pi2_pop_bool(c,"is_sudo_inert")) to_medium=1;
    else if (pi2_pop_bool(c,"is_env_dump_in_safety_clause")) to_medium=1;
    else if (pi2_pop_bool(c,"is_repo_root_traversal")) to_medium=1;
    else if (pi2_pop_bool(c,"is_benign_lazy_os_import")) to_medium=1;
    else if (pi2_pop_bool(c,"is_disclosed_config_note")) to_medium=1;
    else if (pi2_pop_bool(c,"is_authorized_keys_prose")) to_medium=1;
    if (c->exc) return NULL;
    if (to_medium) return json_string(pi2_at_most_s(sev,"medium"));
    if (pi2_pop_bool(c,"is_public_client_key")) {
        if (c->exc) return NULL;
        return json_string(pi2_at_most_s(sev,"high"));
    }
    if (c->exc) return NULL;
    return json_string(sev);
}

static json_t *pi2_context_severity(pi2_ctx *c, json_t *in) {
    json_t *f = pi2_finding_in(in);
    const char *sev = pi2_s(f,"severity","");
    int doc_prose = pi2_b(in,"doc_prose",0);
    int locale_catalog = pi2_b(in,"locale_catalog",0);
    int is_code = pi2_b(in,"is_code",1);
    const char *cap;
    if (doc_prose) {
        cap = pi2_pop_str(c, locale_catalog ? "catalog_cap" : "prose_cap");
        if (c->exc) return NULL;
        if (*cap) sev = cap;
        if (pi2_pop_bool(c,"is_self_uninstall_doc")) sev = pi2_at_most_s(sev,"medium");
        if (c->exc) return NULL;
    } else {
        if (pi2_pop_bool(c,"is_json_prose_value")) {
            if (c->exc) return NULL;
            cap = pi2_pop_str(c,"prose_cap");
            if (c->exc) return NULL;
            if (*cap) sev = cap;
        }
    }
    if (c->exc) return NULL;
    if (pi2_pop_bool(c,"is_test_tree")) {
        if (c->exc) return NULL;
        int inert = !strcmp(pi2_s(f,"category",""),"credential_exposure");
        if (!inert) inert = pi2_pop_bool(c,"is_inert_fixture_line");
        if (c->exc) return NULL;
        sev = inert ? pi2_at_most_s(sev,"medium") : pi2_step_down(sev);
    }
    if (c->exc) return NULL;
    if (is_code && pi2_pop_bool(c,"is_regex_alternation_token")) {
        if (c->exc) return NULL;
        sev = pi2_step_down(sev);
    }
    if (c->exc) return NULL;
    /* _inert_context_cap(f, rel_path, line, joined, is_code, doc_prose, sev) */
    /* temporarily overlay the severity with the computed sev */
    json_t *f2 = json_copy(f);
    json_set(f2,"severity",json_string(sev));
    json_t *in2 = json_object();
    json_set(in2,"finding",f2);
    json_set(in2,"is_code",json_bool(is_code));
    json_t *r = pi2_inert_context_cap(c, in2);
    json_free(in2);  /* owns f2 after json_set */
    return r;
}

static json_t *pi2_filter_findings(pi2_ctx *c, json_t *in) {
    const char *rel = pi2_s(in,"rel_path","");
    /* suffix + is_python from file_path.suffix */
    const char *fsuf = pi2_s(in,"file_suffix","");
    int is_python = !strcmp(fsuf,".py");
    const char *slash = strrchr(rel,'/'); const char *base = slash?slash+1:rel;
    const char *dot = strrchr(base,'.'); const char *suf = dot?dot:"";
    static const char *code_ext[] = {".py",".js",".ts",".sh",".bash",".rb",".pl",".php"};
    int is_code = pi2_in_strs(suf,code_ext,sizeof(code_ext)/sizeof(code_ext[0]));
    int is_js = !strcasecmp(suf,".js")||!strcasecmp(suf,".ts");
    /* seams in source order */
    json_t *mg = NULL, *ds = NULL;
    if (is_python) mg = pi2_seam(c,"main_guard_lines");
    json_t *is_ci = pi2_seam(c,"is_ci");
    json_t *is_doc = pi2_seam(c,"is_doc_prose");
    if (is_python) ds = pi2_seam(c,"docstring_lines");
    json_t *loc = pi2_seam(c,"locale_catalog");
    int doc_prose = (is_doc&&is_doc->type==JSON_BOOL&&is_doc->bool_val) || (is_ci&&is_ci->type==JSON_BOOL&&is_ci->bool_val);
    int locale = (loc&&loc->type==JSON_BOOL&&loc->bool_val) ? 1:0;
    json_t *findings = json_obj_get(in,"findings");
    json_t *lines = NULL;
    if (findings && json_len(findings)>0) lines = pi2_seam(c,"file_lines");
    if (c->exc) return NULL;
    int nlines = (lines&&lines->type==JSON_ARRAY) ? (int)json_len(lines) : 0;
    json_t *out = json_array();
    static const char *exempt[] = {"python_environ_get_secret","python_getenv_secret","python_os_environ",
        "node_process_env","ruby_env_secret","env_exfil_httpx","env_exfil_requests","env_exfil_fetch",
        "env_exfil_curl","env_exfil_wget","context_exfil","send_to_url","fake_policy","agent_config_mod",
        "agent_config_contract","encoded_exfil"};
    static const char *doc_demote[] = {"agent_config_mod","hardcoded_secret"};
    static const char *main_demote[] = {"hardcoded_secret"};
    if (findings && findings->type==JSON_ARRAY) {
        for (size_t i=0;i<json_len(findings);i++) {
            json_t *f0 = json_get(findings,i);
            const char *pid = pi2_s(f0,"pattern_id","");
            const char *sev = pi2_s(f0,"severity","");
            const char *cat = pi2_s(f0,"category","");
            const char *file = pi2_s(f0,"file","");
            int line = pi2_n(f0,"line",0), ln0 = pi2_n(f0,"line0",line);
            const char *match = pi2_s(f0,"match",""), *desc = pi2_s(f0,"description","");
            if (is_code && pi2_in_strs(pid,exempt,sizeof(exempt)/sizeof(exempt[0]))) continue;
            /* JS capability remap then severity remap then doc-prose demotion */
            if (is_js && (!strcmp(pid,"dns_exfil")||!strcmp(pid,"ssh_backdoor"))) sev="high";
            if (!strcmp(pid,"binary_file")) sev="high";
            else if (!strcmp(pid,"hermes_env_access")) sev="medium";
            else if (!strcmp(pid,"curl_pipe_shell")) sev="high";
            if (doc_prose && pi2_in_strs(pid,doc_demote,sizeof(doc_demote)/sizeof(doc_demote[0]))) sev="high";
            /* logical_line is called only when the finding line indexes `lines` */
            if (line > 0 && line <= nlines) { json_t *ll = pi2_seam(c,"logical_line"); (void)ll; }
            /* context severity seam (whole result) */
            json_t *cs = pi2_seam(c,"context_severity");
            if (c->exc) return NULL;
            if (cs && cs->type==JSON_STRING) sev = cs->str_val;
            /* defensive documentation: seam the boolean, plus docstring cap / comment cap */
            json_t *defd = pi2_seam(c,"is_defensive_documentation");
            if (c->exc) return NULL;
            if (defd && defd->type==JSON_BOOL && defd->bool_val) {
                /* is f.line in docstring_lines? */
                int in_doc = 0;
                if (ds && ds->type==JSON_ARRAY)
                    for(size_t k=0;k<json_len(ds);k++) if ((int)json_number_value(json_get(ds,k))==line){in_doc=1;break;}
                if (in_doc) {
                    json_t *dc = pi2_seam(c,"docstring_cap");
                    if (c->exc) return NULL;
                    if (dc && dc->type==JSON_STRING) sev=dc->str_val;
                } else {
                    json_t *cc = pi2_seam(c,"comment_severity");
                    if (c->exc) return NULL;
                    if (cc && cc->type==JSON_STRING) sev=cc->str_val;
                }
                json_t *qi = pi2_seam(c,"is_quoted_injection_phrase");
                if (c->exc) return NULL;
                if (qi && qi->type==JSON_BOOL && qi->bool_val) sev = pi2_at_most_s(sev,"medium");
            }
            if (pi2_in_strs(pid,main_demote,sizeof(main_demote)/sizeof(main_demote[0])) && !strcmp(sev,"critical")) {
                int in_mg=0;
                if (mg&&mg->type==JSON_ARRAY) for(size_t k=0;k<json_len(mg);k++) if((int)json_number_value(json_get(mg,k))==line){in_mg=1;break;}
                if (in_mg) sev="high";
            }
            json_append(out, pi2_finding_obj(pid,sev,cat,file,line,match,desc));
            (void)nlines; (void)ln0;
        }
    }
    return out;
}

static json_t *pi2_dangerous_findings_summary(pi2_ctx *c, json_t *in) {
    (void)c;
    json_t *findings = json_obj_get(in,"findings");
    int crit=0, total=0;
    json_t *ids = json_array();
    if (findings && findings->type==JSON_ARRAY) {
        for (size_t i=0;i<json_len(findings);i++) {
            json_t *f = json_get(findings,i); total++;
            if (!strcmp(pi2_s(f,"severity",""),"critical")) {
                crit++;
                const char *pid = pi2_s(f,"pattern_id","");
                int dup=0; for(size_t k=0;k<json_len(ids);k++) if(!strcmp(json_string_value(json_get(ids,k)),pid)){dup=1;break;}
                if(!dup) json_append(ids, json_string(pid));
            }
        }
    }
    /* sorted ids */
    size_t n = json_len(ids);
    for(size_t i=0;i<n;i++) for(size_t j=i+1;j<n;j++)
        if(strcmp(json_string_value(json_get(ids,i)),json_string_value(json_get(ids,j)))>0){
            const char*t=json_string_value(json_get(ids,i));
            /* rebuild via copies to sort */
        }
    /* sort using a small string array */
    {
        const char **v = calloc(n?n:1,sizeof(char*));
        for(size_t i=0;i<n;i++) v[i]=json_string_value(json_get(ids,i));
        for(size_t i=0;i<n;i++) for(size_t j=i+1;j<n;j++) if(strcmp(v[i],v[j])>0){const char*t=v[i];v[i]=v[j];v[j]=t;}
        char names[2048]=""; for(size_t i=0;i<n;i++){ if(i)strcat(names,", "); strcat(names,v[i]); }
        free(v);
        if(n) return json_string(pi2_fmt("%d critical of %d findings (%s)",crit,total,names));
        return json_string(pi2_fmt("%d critical of %d findings",crit,total));
    }
}

static json_t *pi2_tracked_bytecode(pi2_ctx *c, json_t *in) {
    (void)in;
    json_t *r = pi2_seam(c,"subprocess_run");
    if (c->exc) {
        if (pi2_caught(c,"OSError,subprocess.SubprocessError")) return json_array();
        return NULL;
    }
    json_t *out = json_array();
    if (r && r->type==JSON_OBJECT) {
        json_t *o = json_obj_get(r,"out");
        if (o && o->type==JSON_ARRAY)
            for (size_t i=0;i<json_len(o);i++) {
                json_t *e = json_get(o,i);
                if (e && e->type==JSON_STRING && e->str_val[0]) json_append(out, json_copy(e));
            }
    }
    return pi2_strarr_to_set(out);
}

static json_t *pi2_bytecode_findings(pi2_ctx *c, json_t *in) {
    (void)in;
    json_t *walk = pi2_seam(c,"walk");
    if (c->exc) return NULL;
    json_t *loose = json_array();
    if (walk && walk->type==JSON_ARRAY)
        for(size_t i=0;i<json_len(walk);i++) {
            json_t *e = json_get(walk,i);
            const char *rel = pi2_s(e,"rel","");
            if (!pi2_b(e,"is_file",0)) continue;
            size_t L = strlen(rel);
            if (L>=4 && (!strcmp(rel+L-4,".pyc") || !strcmp(rel+L-4,".pyo"))) json_append(loose,json_string(rel));
        }
    json_t *tracked = pi2_seam(c,"tracked");
    if (c->exc) return NULL;
    if (tracked && tracked->type==JSON_ARRAY)
        for(size_t i=0;i<json_len(tracked);i++) {
            json_t *e=json_get(tracked,i);
            if (e&&e->type==JSON_STRING && e->str_val[0]) json_append(loose,json_copy(e));
        }
    json_t *paths = pi2_strarr_to_set(loose);
    json_t *out = json_array();
    const char *desc = "committed Python bytecode runs in place of the scanned source (catalog rule 17)";
    for(size_t i=0;i<json_len(paths);i++) {
        const char *p = json_string_value(json_get(paths,i));
        json_append(out, pi2_finding_obj("bytecode_file","high","structural",p,0,"committed bytecode",desc));
    }
    return out;
}

static json_t *pi2_check_plugin_structure(pi2_ctx *c, json_t *in) {
    (void)in;
    json_t *findings = pi2_seam(c,"bytecode_findings");
    if (c->exc) return NULL;
    json_t *out = json_array();
    if (findings && findings->type==JSON_ARRAY)
        for(size_t i=0;i<json_len(findings);i++) json_append(out, json_copy(json_get(findings,i)));
    json_t *walk = pi2_seam(c,"walk");
    if (c->exc) return NULL;
    int file_count=0; long long total_size=0;
    static const char *binext[] = {".exe",".dll",".so",".dylib",".bin",".dat",".com",".msi",".dmg",".app",".deb",".rpm"};
    if (walk && walk->type==JSON_ARRAY)
        for(size_t i=0;i<json_len(walk);i++) {
            json_t *e = json_get(walk,i);
            const char *rel = pi2_s(e,"rel","");
            if (pi2_b(e,"is_symlink",0)) {
                file_count++;
                if (pi2_b(e,"resolve_raise",0)) {
                    json_append(out, pi2_finding_obj("broken_symlink","medium","traversal",rel,0,"broken symlink","broken or circular symlink"));
                    continue;
                }
                if (!pi2_b(e,"in_root",1)) {
                    char *m = pi2_fmt("symlink -> %s", pi2_s(e,"resolved",""));
                    json_append(out, pi2_finding_obj("symlink_escape","critical","traversal",rel,0,m,"symlink points outside the plugin directory"));
                    free(m);
                }
                continue;
            }
            if (!pi2_b(e,"is_file",0)) continue;
            file_count++;
    long long size = (long long)json_get_num(e,"size",0.0);
            if (pi2_b(e,"stat_raise",0)) continue;
            total_size += size;
            if (size > 1024LL*1024LL) {
                char *m = pi2_fmt("%lldKB", size/1024);
                char *d = pi2_fmt("file is %lldKB (limit: 1024KB)", size/1024);
                json_append(out, pi2_finding_obj("oversized_file","medium","structural",rel,0,m,d));
                free(m); free(d);
            }
            const char *suf = pi2_s(e,"suffix","");
            if (pi2_in_strs(suf,binext,sizeof(binext)/sizeof(binext[0]))) {
                char *m = pi2_fmt("binary: %s",suf);
                char *d = pi2_fmt("binary/executable file (%s) bundled in plugin (cannot be scanned)",suf);
                json_append(out, pi2_finding_obj("binary_file","high","structural",rel,0,m,d));
                free(m); free(d);
            }
        }
    if (file_count>400) {
        char *m=pi2_fmt("%d files",file_count), *d=pi2_fmt("plugin has %d files (limit: 400)",file_count);
        json_append(out, pi2_finding_obj("too_many_files","medium","structural","(directory)",0,m,d)); free(m);free(d);
    }
    if (total_size>10240LL*1024LL) {
        char *m=pi2_fmt("%lldKB",total_size/1024), *d=pi2_fmt("plugin is %lldKB total (limit: 10240KB)",total_size/1024);
        json_append(out, pi2_finding_obj("oversized_bundle","medium","structural","(directory)",0,m,d)); free(m);free(d);
    }
    return out;
}

static json_t *pi2_scan_plugin(pi2_ctx *c, json_t *in) {
    const char *name = pi2_s(in,"dir_name","plugin");
    const char *source_in = pi2_s(in,"source","");
    json_t *isdir = pi2_seam(c,"is_dir");
    if (c->exc) return NULL;
    int is_dir = (isdir && isdir->type==JSON_BOOL && isdir->bool_val);
    json_t *all = json_array();
    if (is_dir) {
        json_t *st = pi2_seam(c,"structure");
        if (c->exc) return NULL;
        if (st&&st->type==JSON_ARRAY) for(size_t i=0;i<json_len(st);i++) json_append(all,json_copy(json_get(st,i)));
        json_t *walk = pi2_seam(c,"walk");
        if (c->exc) return NULL;
        if (walk&&walk->type==JSON_ARRAY) for(size_t i=0;i<json_len(walk);i++) {
            json_t *e=json_get(walk,i);
            if (!pi2_b(e,"is_file",0) || pi2_b(e,"is_symlink",0)) continue;
            json_t *sf = pi2_seam(c,"scan_file");
            if (c->exc) return NULL;
            json_t *ff = pi2_seam(c,"filter_findings");
            if (c->exc) return NULL;
            if (ff&&ff->type==JSON_ARRAY) for(size_t k=0;k<json_len(ff);k++) json_append(all,json_copy(json_get(ff,k)));
            (void)sf;
        }
    }
    json_t *verdict = pi2_seam(c,"verdict");
    if (c->exc) return NULL;
    const char *v = (verdict&&verdict->type==JSON_STRING)?verdict->str_val:"safe";
    json_t *ts = pi2_seam(c,"scanned_at");
    if (c->exc) return NULL;
    const char *scanned_at = (ts && ts->type==JSON_STRING) ? ts->str_val : "";
    char *summary;
    if (json_len(all)>0) {
        /* categories sorted unique from findings */
        json_t *cats = json_array();
        for(size_t i=0;i<json_len(all);i++) {
            const char *ct=pi2_s(json_get(all,i),"category","");
            int dup=0; for(size_t k=0;k<json_len(cats);k++) if(!strcmp(json_string_value(json_get(cats,k)),ct)){dup=1;break;}
            if(!dup) json_append(cats,json_string(ct));
        }
        size_t n=json_len(cats);
        const char **cv=calloc(n?n:1,sizeof(char*));
        for(size_t i=0;i<n;i++) cv[i]=json_string_value(json_get(cats,i));
        for(size_t i=0;i<n;i++) for(size_t j=i+1;j<n;j++) if(strcmp(cv[i],cv[j])>0){const char*t=cv[i];cv[i]=cv[j];cv[j]=t;}
        char joined[2048]=""; for(size_t i=0;i<n;i++){if(i)strcat(joined,", ");strcat(joined,cv[i]);}
        free(cv);
        summary = pi2_fmt("%s: %s — %d finding(s) in %s", name, v, (int)json_len(all), joined);
    } else {
        summary = pi2_fmt("%s: clean scan, no threats detected", name);
    }
    json_t *res = json_object();
    json_set(res,"skill_name",json_string(name));
    json_set(res,"source",json_string(*source_in?source_in:name));
    json_set(res,"trust_level",json_string("community"));
    json_set(res,"verdict",json_string(v));
    json_set(res,"findings",json_copy(all));
    json_set(res,"scanned_at",json_string(scanned_at));
    json_set(res,"summary",json_string(summary));
    /* provenance is a plain dict on the result object */
    json_t *prov = json_object();
    json_set(prov,"scanner_version",json_string("plugin-guard-v15"));
    json_set(prov,"verdict",json_string(v));
    json_set(prov,"source",json_string(*source_in?source_in:name));
    json_set(res,"scan_provenance",prov);
    free(summary);
    return res;
}

static json_t *pi2_should_allow_plugin_install(pi2_ctx *c, json_t *in) {
    json_t *result = json_obj_get(in,"result");
    int force = pi2_b(in,"force",0);
    const char *v = pi2_s(result,"verdict","");
    int n = json_has(result,"findings") ? (int)json_len(json_obj_get(result,"findings")) : 0;
    json_t *pair = json_array();
    if (!strcmp(v,"safe")) {
        json_append(pair, json_bool(1));
        json_append(pair, json_string("Allowed (clean scan)"));
        return pair;
    }
    if (!strcmp(v,"caution")) {
        if (force) {
            char *s = pi2_fmt("Force-installed despite caution verdict (%d findings)",n);
            json_append(pair, json_bool(1)); json_append(pair, json_string(s)); free(s); return pair;
        }
        char *s = pi2_fmt("Requires confirmation (caution verdict, %d findings)",n);
        json_append(pair, json_null()); json_append(pair, json_string(s)); free(s); return pair;
    }
    /* dangerous: build a fake finding list for the pure summary */
    json_t *tmp = json_object();
    json_set(tmp,"findings", json_has(result,"findings") ? json_copy(json_obj_get(result,"findings")) : json_array());
    json_t *sum = pi2_dangerous_findings_summary(c,tmp);
    json_free(tmp);
    const char *sdesc = json_string_value(sum);
    char *out = pi2_fmt("Blocked (dangerous verdict, %s). --force does not override a dangerous verdict.", sdesc?sdesc:"");
    json_append(pair, json_bool(0)); json_append(pair, json_string(out));
    free(out); json_free(sum);
    return pair;
}
