/*
 * t_port_learning_graph.c — Oracle harness for agent/learning_graph.py
 * pure-transform ports (build_edges, density_stats, _memory_skill_edges).
 *
 * Emits one JSON line per case with the INPUTS plus the C output, so the Python
 * oracle replays the identical calls against LIVE Python.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *learning_graph_build_edges(const char *nodes_json);
extern char *learning_graph_density_stats(const char *nodes_json, const char *edges_json);
extern char *learning_graph_memory_skill_edges(const char *cards_json, const char *skills_json);

static void js(const char *s)
{
    /* emit s as a JSON string (or null) */
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

/* Emit {case, in:<json array of input strings>, out:<parsed C output or null>} */
static void emit(const char *name, const char *const *inputs, int ninput, const char *raw_json)
{
    json_t *o = json_object();
    json_set(o, "case", json_string(name));
    json_t *in = json_new_array();
    for (int i = 0; i < ninput; i++) {
        json_t *parsed = json_parse(inputs[i] ? inputs[i] : "null", NULL);
        json_array_append(in, parsed ? parsed : json_null());
    }
    json_set(o, "in", in);
    json_t *parsed = json_parse(raw_json ? raw_json : "null", NULL);
    json_set(o, "out", parsed ? parsed : json_null());
    char *s = json_serialize(o);
    printf("%s\n", s);
    free(s);
    json_free(o);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    const char *const *ins;

    /* ---- build_edges ---- */
    const char *nodes1 =
        "["
        "{\"name\":\"alpha\",\"related\":[\"beta\",\"gamma\",\"missing\",\"alpha\"]},"
        "{\"name\":\"beta\",\"related\":[\"alpha\"]},"
        "{\"name\":\"gamma\",\"related\":[\"beta\"]},"
        "{\"name\":\"delta\",\"related\":[]}"
        "]";
    ins = (const char *const[]){ nodes1 };
    { char *e = learning_graph_build_edges(nodes1); emit("build_edges_basic", ins, 1, e); free(e); }

    const char *nodes2 = "[]";
    ins = (const char *const[]){ nodes2 };
    { char *e = learning_graph_build_edges(nodes2); emit("build_edges_empty", ins, 1, e); free(e); }

    /* ---- density_stats ---- */
    const char *dnodes =
        "["
        "{\"name\":\"alpha\",\"category\":\"web\",\"use_count\":3,\"created_by\":\"agent\"},"
        "{\"name\":\"beta\",\"category\":\"web\",\"use_count\":0,\"created_by\":\"user\"},"
        "{\"name\":\"gamma\",\"category\":\"data\",\"use_count\":5,\"created_by\":\"agent\"},"
        "{\"name\":\"delta\",\"category\":\"ops\",\"use_count\":0,\"created_by\":null}"
        "]";
    const char *dedges = "[[\"alpha\",\"beta\"],[\"beta\",\"gamma\"]]";
    ins = (const char *const[]){ dnodes, dedges };
    { char *d = learning_graph_density_stats(dnodes, dedges); emit("density_basic", ins, 2, d); free(d); }

    ins = (const char *const[]){ "[]", "[]" };
    { char *d = learning_graph_density_stats("[]", "[]"); emit("density_empty", ins, 2, d); free(d); }

    /* ---- memory_skill_edges ---- */
    const char *cards =
        "["
        "{\"source\":\"memory\",\"fingerprint\":\"326938da6797\",\"title\":\"web scraping notes\",\"body\":\"alpha helps with scraping web pages\"},"
        "{\"source\":\"profile\",\"fingerprint\":\"078e1cc4832e\",\"title\":\"data pipeline\",\"body\":\"gamma runs the data pipeline nightly\"}"
        "]";
    const char *mskills =
        "["
        "{\"name\":\"alpha\"},"
        "{\"name\":\"gamma\"},"
        "{\"name\":\"scraping\"},"
        "{\"name\":\"pipeline\"}"
        "]";
    ins = (const char *const[]){ cards, mskills };
    { char *m = learning_graph_memory_skill_edges(cards, mskills); emit("mem_edges_basic", ins, 2, m); free(m); }

    ins = (const char *const[]){ "[]", mskills };
    { char *m = learning_graph_memory_skill_edges("[]", mskills); emit("mem_edges_empty", ins, 2, m); free(m); }

    return 0;
}

