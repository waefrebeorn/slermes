/*
 * gw_notifier.c — gateway background maintenance threads.
 * Extracted from gateway/server.c (monolith split): the kanban
 * notification watcher (mirrors Python _kanban_notifier_watcher) and the
 * idle-session cleanup thread.
 */

#include "gw_pollers.h"
#include "gw_server_internals.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

/* GW13: Kanban notifier thread function
 * Polls kanban board JSON files for pending notification events and
 * delivers them to subscribed platform/chat/thread targets via the
 * gateway's platform send function. Mirrors Python's _kanban_notifier_watcher.
 *
 * For each board on disk (~/.hermes/kanban/boards/<slug>.json):
 *   1. Read notify_subs and find active subscriptions owned by this profile
 *   2. For each sub, find terminal events (completed/blocked/gave_up/crashed/timed_out)
 *      with id > last_event_id for the subscribed task
 *   3. Send notification message to (platform, chat_id, thread_id)
 *   4. Advance cursor on success, increment fail_count on failure
 *   5. Remove sub after max consecutive failures (dead chat detection)
 *   6. Auto-unsubscribe when task reaches terminal state (done/archived)
 */
void *thread_kanban_notifier(void *arg) {
    (void)arg;

    /* Initial delay so gateway finishes wiring platform adapters */
    sleep(5);

    fprintf(stderr, "[kanban-notifier] started (profile=%s)\n",
            g_gw.kanban_notifier_profile[0] ? g_gw.kanban_notifier_profile : "(default)");

    while (g_gw.running) {
        sleep(g_gw.kanban_notifier_interval_sec);
        if (!g_gw.running) break;

        /* Scan board files on disk */
        char boards_dir[4096];
        snprintf(boards_dir, sizeof(boards_dir), "%s/.hermes/kanban/boards",
                 getenv("HOME") ? getenv("HOME") : "/tmp");

        DIR *dir = opendir(boards_dir);
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (!strstr(entry->d_name, ".json")) continue;

            char board_path[4096];
            snprintf(board_path, sizeof(board_path), "%s/%s", boards_dir, entry->d_name);

            /* Read board JSON file */
            FILE *f = fopen(board_path, "r");
            if (!f) continue;

            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            if (fsize > 1024 * 1024 || fsize <= 0) { fclose(f); continue; }
            fseek(f, 0, SEEK_SET);

            char *json_buf = (char *)malloc((size_t)fsize + 1);
            if (!json_buf) { fclose(f); continue; }
            size_t nread = fread(json_buf, 1, (size_t)fsize, f);
            json_buf[nread] = '\0';
            fclose(f);

            /* Parse board JSON — extract slug, tasks, events, notify_subs */
            json_t *board = json_parse(json_buf, NULL);
            free(json_buf);
            if (!board) continue;

            const char *slug = json_get_str(board, "slug", "default");
            (void)slug;

            /* Process notify_subs */
            json_t *subs = json_obj_get(board, "notify_subs");
            json_t *events = json_obj_get(board, "events");

            if (subs && events) {
                size_t nsubs = json_len(subs);
                size_t nevents = json_len(events);

                for (size_t si = 0; si < nsubs; si++) {
                    json_t *sub = json_get(subs, si);
                    if (!sub) continue;
                    if (!json_get_bool(sub, "active", false)) continue;

                    const char *task_id = json_get_str(sub, "task_id", "");
                    const char *platform = json_get_str(sub, "platform", "");
                    const char *chat_id = json_get_str(sub, "chat_id", "");
                    const char *thread_id = json_get_str(sub, "thread_id", "");
                    const char *sub_profile = json_get_str(sub, "notifier_profile", "");
                    int64_t last_event_id = (int64_t)json_get_num(sub, "last_event_id", 0);

                    if (!task_id[0] || !platform[0] || !chat_id[0]) continue;

                    /* Profile ownership check */
                    if (sub_profile[0] && g_gw.kanban_notifier_profile[0] &&
                        strcmp(sub_profile, g_gw.kanban_notifier_profile) != 0) {
                        continue;  /* owned by different profile */
                    }

                    /* Check if platform adapter is connected */
                    gw_platform_t *plat = gw_platform_find(platform);
                    if (!plat) continue;

                    /* Collect unseen terminal events for this task */
                    int64_t max_id = last_event_id;
                    int event_count = 0;
                    char event_kinds[64][64];
                    char event_payloads[64][2048];

                    for (size_t ei = 0; ei < nevents; ei++) {
                        json_t *ev = json_get(events, ei);
                        if (!ev) continue;

                        int64_t ev_id = (int64_t)json_get_num(ev, "id", 0);
                        if (ev_id <= last_event_id) continue;

                        const char *ev_task = json_get_str(ev, "task_id", "");
                        if (strcmp(ev_task, task_id) != 0) continue;

                        const char *kind = json_get_str(ev, "kind", "");
                        /* Only terminal kinds */
                        bool is_terminal = false;
                        const char *terminal_kinds[] = {
                            "completed", "blocked", "gave_up", "crashed", "timed_out", NULL
                        };
                        for (int tk = 0; terminal_kinds[tk]; tk++) {
                            if (strcmp(kind, terminal_kinds[tk]) == 0) {
                                is_terminal = true;
                                break;
                            }
                        }
                        if (!is_terminal) continue;

                        if (event_count < 64) {
                            snprintf(event_kinds[event_count], 64, "%s", kind);
                            const char *payload = json_get_str(ev, "payload", "{}");
                            snprintf(event_payloads[event_count], 2048, "%s", payload);
                            event_count++;
                        }
                        if (ev_id > max_id) max_id = ev_id;
                    }

                    if (event_count == 0) continue;

                    /* Build notification message */
                    char msg[4096];
                    if (event_count == 1) {
                        snprintf(msg, sizeof(msg),
                                 "📋 Kanban task %s: %s\n%s",
                                 task_id, event_kinds[0], event_payloads[0]);
                    } else {
                        int written = snprintf(msg, sizeof(msg),
                                               "📋 Kanban task %s: %d new events\n",
                                               task_id, event_count);
                        for (int ei = 0; ei < event_count && ei < 5; ei++) {
                            size_t len = strlen(msg);
                            snprintf(msg + len, sizeof(msg) - len,
                                     "  • %s\n", event_kinds[ei]);
                        }
                        (void)written;
                    }

                    /* Deliver via platform adapter */
                    char full_chat[256];
                    if (thread_id && thread_id[0]) {
                        snprintf(full_chat, sizeof(full_chat), "%s:%s", chat_id, thread_id);
                    } else {
                        snprintf(full_chat, sizeof(full_chat), "%s", chat_id);
                    }

                    int sent = gw_platform_send(platform, full_chat, msg) ? 0 : -1;

                    if (sent == 0) {
                        /* Success: advance cursor in the JSON file */
                        json_set(sub, "last_event_id", json_number((double)max_id));

                        /* Check if task is terminal — auto-unsubscribe */
                        json_t *task_list = json_obj_get(board, "tasks");
                        if (task_list) {
                            size_t ntasks = json_len(task_list);
                            for (size_t ti = 0; ti < ntasks; ti++) {
                                json_t *t = json_get(task_list, ti);
                                if (!t) continue;
                                const char *tid = json_get_str(t, "id", "");
                                if (strcmp(tid, task_id) == 0) {
                                    const char *col = json_get_str(t, "column", "");
                                    if (strcmp(col, "done") == 0 || strcmp(col, "archived") == 0) {
                                        json_set(sub, "active", json_bool(false));
                                    }
                                    break;
                                }
                            }
                        }

                        fprintf(stderr, "[kanban-notifier] delivered %d event(s) for %s to %s:%s\n",
                                event_count, task_id, platform, full_chat);
                    } else {
                        /* Failure: increment fail count */
                        int fail_count = (int)json_get_num(sub, "fail_count", 0) + 1;
                        json_set(sub, "fail_count", json_number((double)fail_count));

                        if (fail_count >= g_gw.kanban_notifier_max_fail) {
                            json_set(sub, "active", json_bool(false));
                            fprintf(stderr, "[kanban-notifier] removed dead sub for %s on %s (fail_count=%d)\n",
                                    task_id, platform, fail_count);
                        } else {
                            fprintf(stderr, "[kanban-notifier] send failed for %s on %s (fail_count=%d/%d)\n",
                                    task_id, platform, fail_count, g_gw.kanban_notifier_max_fail);
                        }
                    }
                }
            }

            /* Write updated board back to disk */
            char *updated = json_serialize(board);
            if (updated) {
                FILE *out = fopen(board_path, "w");
                if (out) {
                    fputs(updated, out);
                    fclose(out);
                }
                free(updated);
            }

            json_free(board);
        }
        closedir(dir);
    }

    fprintf(stderr, "[kanban-notifier] stopped\n");
    return NULL;
}

void *thread_cleanup_sessions(void *arg) {
    (void)arg;
    while (g_gw.running) {
        sleep(60);
        pthread_mutex_lock(&g_gw.session_mutex);
        session_cleanup_idle();
        pthread_mutex_unlock(&g_gw.session_mutex);
    }
    return NULL;
}
