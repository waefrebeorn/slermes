/*
 * t_port_learning_graph.c — Oracle harness for agent/learning_graph.py
 * pure-transform ports (build_edges, density_stats, _memory_skill_edges).
 *
 * Emits one JSON line per case (case name + C output). The Python oracle
 * replays the identical calls against LIVE Python and compares (structural,
 * order-insensitive where Python's is).
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *learning_graph_build_edges(const char *nodes_json);
extern char *learning_graph_density_stats(const char *nodes_json, const char *edges_json);
extern char *learning_graph_memory_skill_edges(const char *cards_json, const char *skills_json);

static void emit(const char *name, const char *raw_json)
{
    /* raw_json is already valid JSON text produced by the C fn; wrap it */
    json_t *o = json_object();
    json_set(o, "case", json_string(name));
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

    /* ---- build_edges ---- */
    /* nodes with related_skills; only edges where BOTH endpoints exist,
       sorted, deduped, no self-loops. */
    const char *nodes1 =
        "["
        "{\"name\":\"alpha\",\"related\":[\"beta\",\"gamma\",\"missing\",\"alpha\"]},"
        "{\"name\":\"beta\",\"related\":[\"alpha\"]},"
        "{\"name\":\"gamma\",\"related\":[\"beta\"]},"
        "{\"name\":\"delta\",\"related\":[]}"
        "]";
    char *e1 = learning_graph_build_edges(nodes1);
    emit("build_edges_basic", e1);

    const char *nodes2 = "[]";
    char *e2 = learning_graph_build_edges(nodes2);
    emit("build_edges_empty", e2);

    /* ---- density_stats ---- */
    const char *dnodes =
        "["
        "{\"name\":\"alpha\",\"category\":\"web\",\"use_count\":3,\"created_by\":\"agent\"},"
        "{\"name\":\"beta\",\"category\":\"web\",\"use_count\":0,\"created_by\":\"user\"},"
        "{\"name\":\"gamma\",\"category\":\"data\",\"use_count\":5,\"created_by\":\"agent\"},"
        "{\"name\":\"delta\",\"category\":\"ops\",\"use_count\":0,\"created_by\":null}"
        "]";
    const char *dedges = "[[\"alpha\",\"beta\"],[\"beta\",\"gamma\"]]";
    char *d1 = learning_graph_density_stats(dnodes, dedges);
    emit("density_basic", d1);

    char *d2 = learning_graph_density_stats("[]", "[]");
    emit("density_empty", d2);

    /* ---- memory_skill_edges ---- */
    const char *cards =
        "["
        "{\"source\":\"memory\",\"title\":\"web scraping notes\",\"body\":\"alpha helps with scraping web pages\"},"
        "{\"source\":\"profile\",\"title\":\"data pipeline\",\"body\":\"gamma runs the data pipeline nightly\"}"
        "]";
    const char *mskills =
        "["
        "{\"name\":\"alpha\"},"
        "{\"name\":\"gamma\"},"
        "{\"name\":\"scraping\"},"
        "{\"name\":\"pipeline\"}"
        "]";
    char *m1 = learning_graph_memory_skill_edges(cards, mskills);
    emit("mem_edges_basic", m1);

    char *m2 = learning_graph_memory_skill_edges("[]", mskills);
    emit("mem_edges_empty", m2);

    free(e1); free(e2); free(d1); free(d2); free(m1); free(m2);
    return 0;
}
