/* onboarding_reminders.c -- canonical C11 port of agent/onboarding_text.py's reminder layer.
 *
 * The flow decides *where* a run is; this decides what the model reads at that point. Pure: record
 * and facts in, text out.
 *
 * Not ported here: build_guidance (the "build_*" keys). It needs each catalog task's prompt fields and
 * skills, so `onboarding_reminder` returns NULL for those keys rather than inventing text; the
 * differential oracle lists them as skipped, not as passing.
 */
#include "onboarding_reminders.h"
#include "onboarding_task_catalog.h"
#include "hermes_json.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PICKS 4
#define MAX_LABEL 32

/* ── string building ───────────────────────────────────────────────────────────────────────── */

typedef struct { char *s; size_t n, cap; } sb_t;

static void sb_init(sb_t *b) { b->cap = 512; b->n = 0; b->s = malloc(b->cap); if (b->s) b->s[0] = '\0'; }

static void sb_add(sb_t *b, const char *t) {
    if (!b->s || !t) return;
    size_t l = strlen(t);
    if (b->n + l + 1 > b->cap) {
        while (b->n + l + 1 > b->cap) b->cap *= 2;
        char *g = realloc(b->s, b->cap);
        if (!g) return;
        b->s = g;
    }
    memcpy(b->s + b->n, t, l + 1);
    b->n += l;
}

static void sb_addf(sb_t *b, const char *fmt, const char *a) {
    char tmp[1024];
    snprintf(tmp, sizeof tmp, fmt, a ? a : "");
    sb_add(b, tmp);
}

static char *sb_take(sb_t *b) {
    if (!b->s) return NULL;
    char *out = b->s;
    b->s = NULL;
    return out;
}

/* ── record readers ────────────────────────────────────────────────────────────────────────── */

static const json_t *rec_picks(const json_t *record) {
    const json_t *p = record ? json_obj_get(record, "picks") : NULL;
    return (p && p->type == JSON_OBJECT) ? p : NULL;
}

static const char *pick_str(const json_t *record, const char *key) {
    const json_t *picks = rec_picks(record);
    return picks ? json_get_str(picks, key, NULL) : NULL;
}

static const json_t *pick_arr(const json_t *record, const char *key) {
    const json_t *picks = rec_picks(record);
    const json_t *v = picks ? json_obj_get(picks, key) : NULL;
    return (v && v->type == JSON_ARRAY) ? v : NULL;
}

/* next_beat over the same beat order the flow uses. */
static const char *next_beat(const json_t *record) {
    static const char *const ids[] = { "name", "theme", "layout", "connectors", "plugins", "tour", "build", NULL };
    const json_t *beats = record ? json_obj_get(record, "beats") : NULL;
    if (!beats) return NULL;
    for (int i = 0; ids[i]; i++) {
        const char *st = json_get_str(beats, ids[i], "pending");
        if (strcmp(st, "pending") == 0 || strcmp(st, "shown") == 0) return ids[i];
    }
    return NULL;
}

static const char *waiting_beat(const json_t *record) {
    const json_t *w = record ? json_obj_get(record, "waiting") : NULL;
    const char *b = (w && w->type == JSON_OBJECT) ? json_get_str(w, "beat", NULL) : NULL;
    return (b && *b) ? b : next_beat(record);
}

/* ── _names ────────────────────────────────────────────────────────────────────────────────── */

static void sb_names(sb_t *b, const json_t *arr) {
    if (!arr || arr->type != JSON_ARRAY || json_len((json_t *)arr) == 0) { sb_add(b, "none"); return; }
    for (size_t i = 0; i < json_len((json_t *)arr); i++) {
        const json_t *v = json_get(arr, i);
        if (i) sb_add(b, ", ");
        sb_add(b, (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "");
    }
}

/* ── _build_menu / _next ───────────────────────────────────────────────────────────────────── */

static const json_t *seam_rows(const json_t *seam) {
    const json_t *r = seam ? json_obj_get(seam, "eligible_rows") : NULL;
    return (r && r->type == JSON_ARRAY) ? r : NULL;
}

static const char *seam_fixed_label(const json_t *seam) {
    return seam ? json_get_str(seam, "fixed_label", "") : "";
}

static void sb_build_menu(sb_t *b, const json_t *record, const json_t *seam) {
    char head[1400];
    snprintf(head, sizeof head,
             "For the build card, pick up to %d of these that fit what you learned about them, earlier rows "
             "first when they fit, and call setup_beat(beat=\"build\", options=[{\"id\": ..., \"label\": ...}]). "
             "Reword each label to fit them and keep its energy: a short, specific action they would want to "
             "click, %d characters max. Ids only from this list. \"%s\" is added last by itself.\n",
             MAX_PICKS, MAX_LABEL, seam_fixed_label(seam));
    sb_add(b, head);
    const json_t *rows = seam_rows(seam);
    (void)record;
    if (rows) {
        for (size_t i = 0; i < json_len((json_t *)rows); i++) {
            const json_t *r = json_get(rows, i);
            const char *id = json_get_str(r, "id", "");
            const char *label = json_get_str(r, "label", "");
            const char *goal = json_get_str(r, "goal", "");
            char line[1024];
            snprintf(line, sizeof line, "%s- %s (%s): %s.", i ? "\n" : "", id, label, goal);
            sb_add(b, line);
        }
    }
}

static void sb_next(sb_t *b, const json_t *record, const json_t *seam) {
    const char *beat = next_beat(record);
    if (!beat) {
        sb_add(b, "Every beat is settled; after the build is shown and its close card answered, call "
                  "setup_beat(beat=\"done\").");
        return;
    }
    char line[160];
    snprintf(line, sizeof line, "Next: setup_beat(beat=\"next\") shows the %s card.", beat);
    sb_add(b, line);
    if (strcmp(beat, "build") == 0) {
        sb_add(b, " ");
        sb_build_menu(b, record, seam);
    }
}

/* ── individual reminders ──────────────────────────────────────────────────────────────────── */

static const char *LOOK_AROUND =
    "Before the card, offer once to look around this computer and suggest what to connect. Only on yes, and "
    "only installed apps, developer tools, the OS and the hardware; never personal files, documents, browser "
    "data or messages without a specific ask. Then show the card with what you suggest: "
    "setup_beat(beat=\"connectors\", suggest=[<connector slugs>]). If they say no, show it with "
    "setup_beat(beat=\"connectors\").";

static const char *TOUR_CALL =
    "tool_call {\"calls\":[{\"name\":\"gui_tour\",\"arguments\":{\"action\":\"start\",\"preset\":\"%s\"}}]}";

static int typed_answer(char *out, size_t sz, const char *beat) {
    if (strcmp(beat, "name") == 0) { snprintf(out, sz, "setup_beat(beat=\"name\", value=\"<their name>\")"); return 1; }
    if (strcmp(beat, "theme") == 0) {
        snprintf(out, sz, "tool_call {\"calls\":[{\"name\":\"set_theme\",\"arguments\":{\"accent\":\"<colour>\"}}]}");
        return 1;
    }
    if (strcmp(beat, "layout") == 0) {
        snprintf(out, sz, "tool_call {\"calls\":[{\"name\":\"set_theme\",\"arguments\":{\"layout\":\"sidebar-left\" "
                          "or \"terminal-deck\"}}]}");
        return 1;
    }
    if (strcmp(beat, "connectors") == 0) {
        snprintf(out, sz, "setup_beat(beat=\"connectors\", value=\"continue\") for \"done\" or \"continue\", "
                          "skip=true for \"skip\"");
        return 1;
    }
    if (strcmp(beat, "plugins") == 0) {
        snprintf(out, sz, "setup_beat(beat=\"plugins\", value=\"continue\") for \"done\" or \"continue\", "
                          "skip=true for \"skip\"");
        return 1;
    }
    if (strcmp(beat, "tour") == 0) {
        snprintf(out, sz, "setup_beat(beat=\"tour\", value=\"quick\", \"full\" or \"skip\"), bucketing their "
                          "words into one of the three");
        return 1;
    }
    if (strcmp(beat, "build") == 0) {
        snprintf(out, sz, "setup_beat(beat=\"build\", value=<id>) for an option on the card; a task of their "
                          "own is setup_beat(beat=\"build\", value=\"own\"), then build it");
        return 1;
    }
    return 0;
}

static char *reminder_after_layout(const json_t *record, const json_t *seam) {
    const json_t *picks = rec_picks(record);
    const json_t *lv = picks ? json_obj_get(picks, "layout") : NULL;
    const char *layout = (lv && lv->type == JSON_STRING) ? lv->str_val : NULL;
    const char *label = "None";
    if (!layout || !*layout) label = (lv && lv->type == JSON_STRING) ? layout : "None";
    else if (strcmp(layout, "sidebar-left") == 0) label = "Basic";
    else if (strcmp(layout, "terminal-deck") == 0) label = "Elite";
    else label = layout;
    sb_t b; sb_init(&b);
    char head[256];
    snprintf(head, sizeof head, "Layout %s is on, already applied. Do not call set_theme for it again. ", label);
    sb_add(&b, head);
    sb_next(&b, record, seam);
    return sb_take(&b);
}

static char *reminder_tour(const char *preset, const json_t *record, const json_t *seam) {
    sb_t b; sb_init(&b);
    char call[512];
    snprintf(call, sizeof call, TOUR_CALL, preset);
    char head[1200];
    snprintf(head, sizeof head,
             "Say \"Here's where things live.\" Then, in this same turn, call %s and then show the build card: "
             "it waits under the tour. Never bring the tour up again. ", call);
    sb_add(&b, head);
    sb_build_menu(&b, record, seam);
    return sb_take(&b);
}

static char *reminder_wrote(const json_t *record, const char *beat) {
    sb_t b; sb_init(&b);
    char head[512];
    snprintf(head, sizeof head,
             "The %s card closed unanswered: they wrote a message instead. It comes next as their own turn; "
             "write nothing until it does.", beat);
    sb_add(&b, head);
    (void)record;
    if (strcmp(beat, "theme") == 0 || strcmp(beat, "layout") == 0) {
        char typed[512] = "";
        typed_answer(typed, sizeof typed, beat);
        char tail[900];
        snprintf(tail, sizeof tail,
                 " If it asks what you think or which one fits, pick the %s yourself: look around first if they "
                 "asked, then say your pick and why in one line and apply it (%s).", beat, typed);
        sb_add(&b, tail);
    }
    return sb_take(&b);
}

static char *reminder_timed_out(const char *beat) {
    sb_t b; sb_init(&b);
    char text[1200];
    snprintf(text, sizeof text,
             "The %s card closed without an answer. If a message from them comes next, answer it first. "
             "Otherwise say one short line that they can pick up whenever they are back, then stop. When they "
             "return, bring it back with setup_beat(beat=\"%s\").", beat, beat);
    sb_add(&b, text);
    return sb_take(&b);
}


/* ── build_guidance: the prompt once a first task is picked ─────────────────────────────────── */

#define NPM_URL "https://cdn.jsdelivr.net/npm/"

static const char *CLOSE_LINE =
    "When the first thing is built and shown, ask with clarify: \"How does this look?\" with the choices "
    "\"Looks right\", \"Change something\" and \"Take it further\", exactly. Put the next-step idea in your text. "
    "After they answer, call setup_beat(beat=\"done\").";

static const char *HOW_TO_WORK[] = {
    "Real data only: this computer, their connected apps, the live web. Never mock or invent data.",
    "First reply: one line, then a tool call. At most two checks and one question before you start. "
    "Something visible within a minute, finished within five.",
    "Save every file in ~/hermes-first-task/ and say where. No test files.",
    "Install or connect anything once, with exact ids. Never search the whole disk.",
    "Write the words they read with humanizer: short, plain, specific.",
    NULL
};

static const char *PAGE_LINES[] = {
    "One self-contained HTML file, laid out for the pane (often about 600px wide) and able to widen.",
    "A standalone page: it sets its own background, font and colours. The preview pane does not theme it; "
    "the theme variables are only for ::preview widgets in the chat.",
    "It moves within a second: a diagram that draws itself, a dot that runs a path, numbers that count up to "
    "real values.",
    "Libraries, only what the page needs, each pinned as " NPM_URL "<package>@<version>/<file>: gsap@3.13.0 for "
    "motion, with its DrawSVGPlugin, MotionPathPlugin and ScrollTrigger (register each); d3@7.9.0 for charts of "
    "real numbers; p5@1.11.3 for a 2D figure they drag or tune; matter-js@0.20.0 for physics; katex@0.16.11 "
    "with its CSS and auto-render for maths; tone@15.0.4 for sound, started on a click; roughjs@4.6.6 for a "
    "hand-drawn look.",
    "3D: three@0.170.0 is ES modules only. Use an import map: \"three\" to its build/three.module.js, "
    "\"three/addons/\" to its examples/jsm/.",
    NULL
};

static const char *CONNECTORS_LINE =
    "Never route around a connector. "
    "Reading a connected app: check it with manage_connections(action=\"status\"). Find its tools with tool_search "
    "using the app slug, then the slug plus an action (a bare slug misses actions). tool_describe every action you "
    "need in one batch, then call them through tool_call(calls=[{\"name\": \"connectors__<slug>__<ACTION>\", "
    "\"arguments\": {...}}]). Resolve names to ids before acting. Read only; ask before any write, and say that a "
    "write may run as the connector's bot.";

static const char *OS_MACOS =
    "This Mac: check with `which brew` and `ls -d /Applications/<App>.app`; install with "
    "`brew install --cask <app>`; on Apple silicon check `lipo -archs` for a binary's architecture.";
static const char *OS_WINDOWS =
    "This PC: check with `winget list --accept-source-agreements`; install with `winget install <id> "
    "--accept-source-agreements --accept-package-agreements` (without both flags a fresh profile hangs "
    "on an agreements prompt).";
static const char *OS_LINUX =
    "This Linux machine: check with `which`, and `flatpak list` when flatpak is there; install with "
    "`flatpak install -y flathub <id>`. Anything that needs sudo is listed for the user, never run.";
static const char *ARM_WINDOWS =
    "On Windows on Arm, read arm64 vs x64 from winget's install output, not the manifest, and say when "
    "an app is x64 only.";
static const char *SPARK_LINE = "This machine is built for local models; say so once when it fits what they asked.";
static const char *REMOTE_LINE =
    "Hermes runs on another computer than the user's: say which machine anything is installed on.";

static const char *PREVIEW_CALL =
    "tool_call {\"calls\":[{\"name\":\"desktop_preview\",\"arguments\":{\"action\":\"open\",\"url\":\"<file>\"}}]}";

static bool in_list(const char *const *list, const char *v) {
    if (!list || !v) return false;
    for (int i = 0; list[i]; i++) if (strcmp(list[i], v) == 0) return true;
    return false;
}

static void sb_bullets(sb_t *b, const char *title, const char *const *lines) {
    sb_add(b, title);
    sb_add(b, ":");
    for (int i = 0; lines && lines[i]; i++) {
        sb_add(b, "\n- ");
        sb_add(b, lines[i]);
    }
}

/* _preview_line: every host shows results in the app's preview pane; remote hosts do not reload it. */
static void sb_preview_line(sb_t *b, const json_t *facts) {
    const char *src = facts ? json_get_str(facts, "facts_source", "local") : "local";
    sb_add(b, "Show results in the preview pane beside the chat with ");
    sb_add(b, PREVIEW_CALL);
    sb_add(b, "; it shows HTML, images and PDFs. ");
    if (strcmp(src, "remote") == 0)
        sb_add(b, "On this host it does not reload by itself: open it again after each change.");
    else
        sb_add(b, "Open it once, early: it reloads when the file changes, so keep improving the same file "
                  "while they watch.");
    sb_add(b, " Say it is open only after the call succeeds.");
}

/* task_skills with the identity resolver upstream defaults to: an entry a|b keeps every name. */
static void sb_skills(sb_t *b, const onboarding_task_t *task) {
    bool first = true;
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            if (!first) sb_add(b, ", ");
            sb_add(b, ONB_EVERY_TASK_SKILL);
            first = false;
            break;
        }
        for (int i = 0; task->skills[i]; i++) {
            const char *entry = task->skills[i];
            char buf[256];
            snprintf(buf, sizeof buf, "%s", entry);
            char *save = NULL;
            for (char *tok = strtok_r(buf, "|", &save); tok; tok = strtok_r(NULL, "|", &save)) {
                if (!first) sb_add(b, ", ");
                sb_add(b, tok);
                first = false;
            }
        }
    }
}

static void sb_task_prompt(sb_t *b, const onboarding_task_t *task, const char *label) {
    const char *shown = (label && *label) ? label : task->label;
    if (shown && *shown) {
        sb_add(b, "They picked \"");
        sb_add(b, shown);
        sb_add(b, "\". ");
    }
    sb_add(b, "Your job: ");
    sb_add(b, task->goal ? task->goal : "");
    sb_add(b, ".\nLoad these skills with skill_view first: ");
    sb_skills(b, task);
    sb_add(b, ".\nSteps:");
    for (int i = 0; task->steps[i]; i++) {
        char line[32];
        snprintf(line, sizeof line, "\n%d. ", i + 1);
        sb_add(b, line);
        sb_add(b, task->steps[i]);
    }
    sb_add(b, "\nOutput: ");
    sb_add(b, task->output ? task->output : "");
    sb_add(b, "\nQuality bar: ");
    sb_add(b, task->quality ? task->quality : "");
    sb_add(b, "\nDone when: ");
    sb_add(b, task->done ? task->done : "");
}

static char *build_guidance(const char *option, const json_t *facts, const char *label) {
    static const char *const PAGE_TASKS[] = {
        "hermes", "explain", "brief", "week", "youtube", "site", "repo", "own", NULL
    };
    static const char *const CONNECTOR_TASKS[] = { "brief", "week", NULL };

    const onboarding_task_t *task = onboarding_catalog_task_or_own(option);
    sb_t b; sb_init(&b);

    sb_task_prompt(&b, task, label);

    sb_add(&b, "\n\n");
    sb_bullets(&b, "How to work", HOW_TO_WORK);
    /* _preview_line rides inside the work list, after the fixed lines. */
    sb_add(&b, "\n- ");
    sb_preview_line(&b, facts);
    if (in_list(CONNECTOR_TASKS, option)) {
        sb_add(&b, "\n- ");
        sb_add(&b, CONNECTORS_LINE);
    }

    if (in_list(PAGE_TASKS, option)) {
        sb_add(&b, "\n\n");
        sb_bullets(&b, "When you make a page", PAGE_LINES);
    }

    const char *os = facts ? json_get_str(facts, "os", "linux") : "linux";
    const char *arch = facts ? json_get_str(facts, "arch", "") : "";
    const char *nvidia = facts ? json_get_str(facts, "nvidia_platform", "") : "";
    const char *src = facts ? json_get_str(facts, "facts_source", "local") : "local";

    const char *host[4];
    int n = 0;
    host[n++] = (strcmp(os, "macos") == 0) ? OS_MACOS
              : (strcmp(os, "windows") == 0) ? OS_WINDOWS : OS_LINUX;
    if (strcmp(os, "windows") == 0 && strcmp(arch, "arm64") == 0) host[n++] = ARM_WINDOWS;
    if (strcmp(nvidia, "rtx_spark") == 0 || strcmp(nvidia, "dgx_spark") == 0) host[n++] = SPARK_LINE;
    if (strcmp(src, "remote") == 0) host[n++] = REMOTE_LINE;
    host[n] = NULL;

    sb_add(&b, "\n\n");
    sb_bullets(&b, "This computer", host);

    sb_add(&b, "\n\nWhen it is shown:\n");
    sb_add(&b, CLOSE_LINE);

    return sb_take(&b);
}

/* ── public entry points ──────────────────────────────────────────────────────────────────── */

char *onboarding_reminder(const char *key, const json_t *record, const json_t *facts,
                          const char *beat, const json_t *seam) {
    if (!key || !*key) return strdup("");

    if (strcmp(key, "wrote") == 0 || strcmp(key, "timed_out") == 0) {
        const char *b = (beat && *beat) ? beat : waiting_beat(record);
        if (!b || !*b) b = "build";
        return strcmp(key, "wrote") == 0 ? reminder_wrote(record, b) : reminder_timed_out(b);
    }
    if (strncmp(key, "build_", 6) == 0) {
        /* build_<option>: the prompt for the first task the card settled. */
        const char *option = key + 6;
        const char *label = "";
        const json_t *picks = rec_picks(record);
        if (picks) {
            const json_t *opts = json_obj_get(picks, "build_options");
            if (opts && opts->type == JSON_ARRAY) {
                for (size_t i = 0; i < json_len((json_t *)opts); i++) {
                    const json_t *o = json_get(opts, i);
                    const char *id = json_get_str(o, "id", NULL);
                    if (id && strcmp(id, option) == 0) { label = json_get_str(o, "label", ""); break; }
                }
            }
        }
        return build_guidance(option, facts, label);
    }

    if (strncmp(key, "already_", 8) == 0) {
        sb_t b; sb_init(&b);
        char head[512];
        snprintf(head, sizeof head,
                 "The %s was already set by the card or an earlier call. Do not apply it again unless they ask "
                 "for a change. ", key + 8);
        sb_add(&b, head);
        sb_next(&b, record, seam);
        return sb_take(&b);
    }

    if (strcmp(key, "after_name") == 0) {
        sb_t b; sb_init(&b);
        const json_t *picks = rec_picks(record);
        const json_t *nv = picks ? json_obj_get(picks, "name") : NULL;
        const char *name = (nv && nv->type == JSON_STRING) ? (nv->str_val ? nv->str_val : "")
                                                          : "None";
        char head[300];
        snprintf(head, sizeof head, "They go by %s. Greet them by name in a few words. ", name);
        sb_add(&b, head);
        sb_next(&b, record, seam);
        return sb_take(&b);
    }
    if (strcmp(key, "after_theme") == 0) {
        sb_t b; sb_init(&b);
        const char *accent = pick_str(record, "accent");
        char head[400];
        snprintf(head, sizeof head,
                 "Accent %s is on, already applied. Do not call set_theme for it again. React in three words "
                 "or fewer. ", (accent && *accent) ? accent : "Nous blue");
        sb_add(&b, head);
        sb_next(&b, record, seam);
        return sb_take(&b);
    }
    if (strcmp(key, "after_layout") == 0) return reminder_after_layout(record, seam);
    if (strcmp(key, "offer_first") == 0) {
        sb_t b; sb_init(&b);
        sb_add(&b, "The connectors card is not shown yet. ");
        sb_add(&b, LOOK_AROUND);
        return sb_take(&b);
    }
    if (strcmp(key, "after_connectors") == 0) {
        sb_t b; sb_init(&b);
        const json_t *connected = pick_arr(record, "connected");
        char head[400];
        snprintf(head, sizeof head, "Connected: ", "");
        sb_add(&b, head);
        sb_names(&b, connected);
        sb_add(&b, "; newly connected: ");
        sb_names(&b, pick_arr(record, "newly_connected"));
        sb_add(&b, ".");
        if (connected && json_len((json_t *)connected) > 0)
            sb_add(&b, " These are already connected; do not connect them again.");
        sb_add(&b, " ");
        sb_next(&b, record, seam);
        return sb_take(&b);
    }
    if (strcmp(key, "after_plugins") == 0) {
        sb_t b; sb_init(&b);
        const json_t *installed = pick_arr(record, "installed");
        sb_add(&b, "Installed plugins: ");
        sb_names(&b, installed);
        sb_add(&b, ".");
        if (installed && json_len((json_t *)installed) > 0)
            sb_add(&b, " They are already installed; do not install them again.");
        sb_add(&b, " ");
        sb_next(&b, record, seam);
        return sb_take(&b);
    }
    if (strcmp(key, "after_tour") == 0) {
        sb_t b; sb_init(&b);
        sb_add(&b, "The tour is already running; do not start it again. ");
        sb_next(&b, record, seam);
        return sb_take(&b);
    }
    if (strcmp(key, "after_build") == 0) {
        sb_t b; sb_init(&b);
        sb_next(&b, record, seam);
        return sb_take(&b);
    }
    if (strcmp(key, "tour_quick") == 0) return reminder_tour("quick", record, seam);
    if (strcmp(key, "tour_full") == 0) return reminder_tour("full", record, seam);
    if (strcmp(key, "tour_skip") == 0) {
        sb_t b; sb_init(&b);
        sb_add(&b, "No tour. In this same turn, show the build card. ");
        sb_build_menu(&b, record, seam);
        return sb_take(&b);
    }
    if (strcmp(key, "moved_on") == 0) {
        sb_t b; sb_init(&b);
        sb_add(&b, "That beat is skipped; take its default and do not ask it again. ");
        sb_next(&b, record, seam);
        return sb_take(&b);
    }
    if (strcmp(key, "skip_all") == 0)
        return strdup("They ended setup. In one line, say they can ask you anything any time, then stop "
                      "guiding. The onboarding rules no longer apply; this is a normal chat.");
    if (strcmp(key, "complete") == 0)
        return strdup("Onboarding is complete. Its rules no longer apply; this is a normal chat. Close in "
                      "one short line: whenever they are stuck, they can just ask.");
    return NULL;
}

char *onboarding_turn_note(const json_t *record) {
    const char *beat = waiting_beat(record);
    if (!beat) return strdup("");
    char typed[512] = "";
    typed_answer(typed, sizeof typed, beat);
    sb_t b; sb_init(&b);
    char text[1200];
    snprintf(text, sizeof text,
             "[System note: Onboarding continues; the %s card is closed. If their message answers it, apply it "
             "yourself (%s) and move on. Otherwise answer their message first, then bring the card back in one "
             "line with setup_beat(beat=\"%s\").]", beat, typed, beat);
    sb_add(&b, text);
    return sb_take(&b);
}

char *onboarding_finish_reminder(const json_t *record, const json_t *seam) {
    sb_t b; sb_init(&b);
    sb_next(&b, record, seam);
    return sb_take(&b);
}
