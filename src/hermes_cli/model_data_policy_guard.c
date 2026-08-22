/*
 * model_data_policy_guard.c — C11 port of
 * hermes_cli/model_data_policy_guard.py.
 *
 * Static rule table keyed on the vendor-documented model id (the only
 * reliable signal — no training/retention flag is exposed by models.dev or
 * the Meta /v1/models payload). First matching rule wins; a misbehaving
 * predicate must never break model selection.
 */
#define _POSIX_C_SOURCE 200809L
#include "model_data_policy_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *xstrdup0(const char *s)
{
    return strdup(s ? s : "");
}

/* Strip ASCII whitespace in place (Python .strip()). */
static char *str_strip(char *s)
{
    if (!s)
        return s;
    size_t n = strlen(s);
    size_t start = 0;
    while (start < n && (s[start] == ' ' || s[start] == '\t'
                         || s[start] == '\n' || s[start] == '\r'
                         || s[start] == '\f' || s[start] == '\v'))
        start++;
    size_t end = n;
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t'
                           || s[end - 1] == '\n' || s[end - 1] == '\r'
                           || s[end - 1] == '\f' || s[end - 1] == '\v'))
        end--;
    memmove(s, s + start, end - start);
    s[end - start] = '\0';
    return s;
}

static char *str_lower_dup(const char *s)
{
    char *d = xstrdup0(s);
    for (char *p = d; *p; p++)
        if (*p >= 'A' && *p <= 'Z')
            *p = (char)(*p + 32);
    return d;
}

/* PoP: _is_meta_contributor @ hermes_cli/model_data_policy_guard.py:_is_meta_contributor */
/* Meta Model API "contributor" tier. Match the id suffix; do not require a
 * specific provider id. */
static int _is_meta_contributor(const char *model_lower,
                                const char *provider_lower)
{
    (void)provider_lower;
    if (!model_lower)
        return 0;
    size_t n = strlen(model_lower);
    const char *suffix = "-contributor";
    size_t sl = strlen(suffix);
    if (n >= sl && strcmp(model_lower + n - sl, suffix) == 0)
        return 1;
    /* "contributor" in model_lower.split("-") */
    const char *tok = "-contributor-";
    /* token match: model equals, starts with, ends with, or contains the
     * "contributor" dash-token ("contributor" in model.split("-")). */
    if (strcmp(model_lower, "contributor") == 0)
        return 1;
    if (strncmp(model_lower, "contributor-", 12) == 0)
        return 1;
    return strstr(model_lower, tok) != NULL;
}

/* _META_CONTRIBUTOR_MESSAGE (exact Python literal). */
static const char _META_CONTRIBUTOR_MESSAGE[] =
    "!!! CONTRIBUTOR TIER — TRAINS ON YOUR DATA !!!\n"
    "\n"
    "muse-spark-1.2-contributor is Meta's contributor tier: heavily discounted\n"
    "token pricing in exchange for permission to use your prompts and completions\n"
    "to train future Meta models.\n"
    "\n"
    "  Price per 1M tokens:  input $0.10  |  output $0.20  |  cached input $0.002\n"
    "  (vs. standard muse-spark-1.2:  input $1.25  |  output $4.25  |  cached $0.15)\n"
    "\n"
    "It lowers the barrier to entry for prototyping, testing integrations, and\n"
    "scaling experiments where training on your data is acceptable. Do NOT use it\n"
    "for confidential, proprietary, personal, or otherwise sensitive data. For the\n"
    "same model at standard pricing with no training on your data, select the\n"
    "standard variant, muse-spark-1.2.\n"
    "\n"
    "Source: https://dev.meta.ai/docs/pricing-rate-limits/\n"
    "Confirm only if training on your prompts and completions is acceptable.";

/* Rule table: (predicate, message) pairs evaluated in order; first wins. */
typedef int (*mdp_predicate_t)(const char *model_lower, const char *provider_lower);
static const struct {
    mdp_predicate_t predicate;
    const char *message;
} MDP_RULES[] = {
    { _is_meta_contributor, _META_CONTRIBUTOR_MESSAGE },
};

void data_training_warning_free(DataTrainingWarning *w)
{
    if (!w)
        return;
    free(w->model);
    free(w->provider);
    free(w->message);
    free(w);
}

/* PoP: data_training_warning @ hermes_cli/model_data_policy_guard.py:data_training_warning */
DataTrainingWarning *data_training_warning(const char *model_name,
                                           const char *provider,
                                           const char *base_url)
{
    (void)base_url;  /* noqa ARG001 — reserved for host-scoped rules */
    char *model_buf = xstrdup0(model_name);
    str_strip(model_buf);
    if (!model_buf[0]) {
        free(model_buf);
        return NULL;
    }
    char *provider_buf = provider ? xstrdup0(provider) : strdup("");
    str_strip(provider_buf);

    char *model_lower = str_lower_dup(model_buf);
    char *provider_lower = str_lower_dup(provider_buf);

    DataTrainingWarning *result = NULL;
    for (size_t i = 0;
         i < sizeof(MDP_RULES) / sizeof(MDP_RULES[0]); i++) {
        /* Python wraps the predicate call in try/except continue — C
         * predicates are total, so the guard cannot fire here either. */
        if (MDP_RULES[i].predicate(model_lower, provider_lower)) {
            result = malloc(sizeof(*result));
            if (!result)
                break;
            result->model = strdup(model_buf);
            result->provider = strdup(provider_buf);
            result->message = strdup(MDP_RULES[i].message);
            break;   /* first match wins */
        }
    }
    free(model_buf);
    free(provider_buf);
    free(model_lower);
    free(provider_lower);
    return result;
}
