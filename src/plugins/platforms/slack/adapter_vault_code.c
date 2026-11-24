/*
 * adapter_vault_code.c - real C11 port of upstream Hermes
 *   plugins/platforms/slack/adapter_vault_code.py.
 *
 * What the card is made of: the mrkdwn escaping, the author fence, the prompt text
 * (title + body + hint, capped at 3000 characters) and the modal-title/button
 * truncations the Slack API forces. The i18n lookup is the seam: the fixture hands
 * in the templates, so the exact rendered strings are compared.
 *
 * PoP: w19_adaptervau_0f2138_mrkdwn_escape @ plugins/platforms/slack/adapter_vault_code.py:_mrkdwn_escape
 * PoP: w19_adaptervau_0f2138_author_matches @ plugins/platforms/slack/adapter_vault_code.py:_vault_code_author_matches
 * PoP: w19_adaptervau_0f2138_card_text @ plugins/platforms/slack/adapter_vault_code.py:send_vault_code_prompt
 * PoP: w19_adaptervau_0f2138_live_card @ plugins/platforms/slack/adapter_vault_code.py:_live_vault_code_card
 * PoP: w19_adaptervau_0f2138_filter_allows @ plugins/platforms/slack/adapter_vault_code.py:_DropVaultCodeFrames.filter
 */
#include "hermes_json.h"
#include "noc_w19_adaptervau_0f2138.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HINT_LIMIT        300
#define MODAL_TITLE_LIMIT 24
#define CARD_TEXT_LIMIT   3000
#define BUTTON_TEXT_LIMIT 75

static const char *const OPEN_ACTION = "hermes_vault_code_open";
static const char *const VIEW_CALLBACK = "hermes_vault_code_submit";

static const char *jstr(json_t *v) { return (v && v->type == JSON_STRING) ? v->str_val : NULL; }

/* Python truthiness of the values these helpers see. */
static int jtruthy(json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* text.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;"), in that order. */
static char *mrkdwn_escape_dup(const char *s) {
    if (!s) s = "";
    size_t cap = strlen(s) * 5 + 1;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; s[i]; i++) {
        const char *rep = NULL;
        switch (s[i]) {
            case '&': rep = "&amp;"; break;
            case '<': rep = "&lt;"; break;
            case '>': rep = "&gt;"; break;
            default: break;
        }
        if (rep) {
            size_t rl = strlen(rep);
            memcpy(out + o, rep, rl);
            o += rl;
        } else {
            out[o++] = s[i];
        }
    }
    out[o] = '\0';
    return out;
}

/* "{site}".format-style substitution for one keyword (the i18n seam). */
static void format_site(const char *tmpl, const char *site, char *out, size_t n) {
    size_t o = 0;
    const char *p = tmpl ? tmpl : "";
    while (*p && o + 1 < n) {
        if (!strncmp(p, "{site}", 6)) {
            const char *v = site ? site : "";
            size_t vl = strlen(v);
            if (o + vl + 1 > n) break;
            memcpy(out + o, v, vl);
            o += vl;
            p += 6;
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
}

json_t *w19_adaptervau_0f2138_mrkdwn_escape(json_t *text) {
    const char *s = jstr(text);
    char *esc = mrkdwn_escape_dup(s);
    json_t *out;
    if (!esc) return json_null();
    out = json_string(esc);
    free(esc);
    return out;
}

/* user_id == card.user_id and (not card.team_id or not team_id or team_id == card.team_id) */
json_t *w19_adaptervau_0f2138_author_matches(json_t *card_user, json_t *card_team,
                                             json_t *user_id, json_t *team_id) {
    const char *cu = jstr(card_user) ? jstr(card_user) : "";
    const char *ct = jstr(card_team) ? jstr(card_team) : "";
    const char *uid = jstr(user_id) ? jstr(user_id) : "";
    const char *tid = jstr(team_id) ? jstr(team_id) : "";
    int same_user = !strcmp(uid, cu);
    int team_ok = (!ct[0] || !tid[0] || !strcmp(tid, ct));
    return json_bool((same_user && team_ok) ? true : false);
}

/* send_vault_code_prompt's rendered text: body, hint, "*title*", [:3000], plus the block list. */
json_t *w19_adaptervau_0f2138_card_text(json_t *site, json_t *hint, json_t *title_template,
                                        json_t *body_template, json_t *button_template) {
    json_t *out = json_object();
    const char *raw_site = jstr(site) ? jstr(site) : "";
    char *esc_site = mrkdwn_escape_dup(raw_site);
    char title_raw[4096], title_esc[8192], body[16384], text[32768], button[1024];
    char button_cut[BUTTON_TEXT_LIMIT + 1];
    char text_cut[CARD_TEXT_LIMIT + 1];

    if (!esc_site) esc_site = (char *)calloc(1, 1);
    format_site(jstr(title_template), raw_site, title_raw, sizeof title_raw);
    {
        char *esc_title = mrkdwn_escape_dup(title_raw);
        snprintf(title_esc, sizeof title_esc, "%s", esc_title ? esc_title : "");
        free(esc_title);
    }
    format_site(jstr(body_template), esc_site ? esc_site : "", body, sizeof body);
    free(esc_site);

    if (jtruthy(hint)) {
        const char *h = jstr(hint) ? jstr(hint) : "";
        size_t hlen = strlen(h);
        if (hlen > HINT_LIMIT) hlen = HINT_LIMIT;      /* hint[:_HINT_LIMIT] */
        char *head = (char *)malloc(hlen + 1);
        if (head) {
            memcpy(head, h, hlen);
            head[hlen] = '\0';
            char *esc = mrkdwn_escape_dup(head);
            free(head);
            size_t blen = strlen(body);
            if (esc && blen + strlen(esc) + 2 < sizeof body)
                snprintf(body + blen, sizeof body - blen, "\n%s", esc);
            free(esc);
        }
    }
    /* card.text = f"*{escaped title}*\n{body}"[:3000] */
    snprintf(text, sizeof text, "*%s*\n%s", title_esc, body);
    {
        size_t tl = strlen(text);
        if (tl > CARD_TEXT_LIMIT) tl = CARD_TEXT_LIMIT;
        memcpy(text_cut, text, tl);
        text_cut[tl] = '\0';
    }

    format_site(jstr(button_template), raw_site, button, sizeof button);
    {
        size_t bl = strlen(button);
        if (bl > BUTTON_TEXT_LIMIT) bl = BUTTON_TEXT_LIMIT;
        memcpy(button_cut, button, bl);
        button_cut[bl] = '\0';
    }

    json_set(out, "text", json_string(text_cut));
    json_set(out, "title", json_string(title_raw));
    json_set(out, "button", json_string(button_cut));

    json_t *blocks = json_array();
    json_t *section = json_object();
    json_t *sec_text = json_object();
    json_set(sec_text, "type", json_string("mrkdwn"));
    json_set(sec_text, "text", json_string(text_cut));
    json_set(section, "type", json_string("section"));
    json_set(section, "text", sec_text);
    json_append(blocks, section);
    json_t *actions = json_object();
    json_t *elements = json_array();
    json_t *el = json_object();
    json_set(el, "type", json_string("button"));
    json_set(el, "text", json_string(button_cut));
    json_set(el, "action_id", json_string(OPEN_ACTION));
    json_set(el, "style", json_string("primary"));
    json_append(elements, el);
    json_set(actions, "type", json_string("actions"));
    json_set(actions, "elements", elements);
    json_append(blocks, actions);
    json_set(out, "blocks", blocks);
    return out;
}

/* card is live when it exists and now < expires_at */
json_t *w19_adaptervau_0f2138_live_card(json_t *expires_at, double now) {
    int live = jtruthy(expires_at) && now < (expires_at->type == JSON_NUMBER ? expires_at->num_val : 0.0);
    return json_bool(live ? true : false);
}

/* bool(filter(record)) == VIEW_CALLBACK not in record.getMessage() */
json_t *w19_adaptervau_0f2138_filter_allows(json_t *message_text) {
    const char *s = jstr(message_text) ? jstr(message_text) : "";
    return json_bool(strstr(s, VIEW_CALLBACK) ? false : true);
}
