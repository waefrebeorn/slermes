/* t_port_model_picker_usage.c — oracle harness for model_picker_usage.py.
 * Emits one JSON line per case with a "fn" discriminator. */
#include "port_model_picker_usage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *P(const char *s)
{
    return json_parse(s, NULL);
}

static void jstr_escape(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar(ch);
        } else if (ch < 0x20) {
            printf("\\u%04x", ch);
        } else {
            putchar(ch);
        }
    }
    putchar('"');
}

static void emit(int cs, const char *fn, const char *in, json_t *out)
{
    char *s = out ? json_serialize(out) : NULL;
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":", cs, fn, in);
    jstr_escape(s ? s : "null");
    printf("}\n");
    fflush(stdout);
    free(s);
    json_free(out);
}

/* A C string result is wrapped so an empty string cannot be confused with a
 * malformed/empty JSON value; numbers and JSON values go out directly. */
static void emit_str(int cs, const char *fn, const char *in, const char *out)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":{\"str\":", cs, fn, in);
    jstr_escape(out ? out : "");
    printf("}}\n");
    fflush(stdout);
}

static void emit_num(int cs, const char *fn, const char *in, double v)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":%.17g}\n", cs, fn, in, v);
    fflush(stdout);
}

static void emit_int(int cs, const char *fn, const char *in, long v)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":%ld}\n", cs, fn, in, v);
    fflush(stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* clamp_fraction */
    emit_num(1, "clamp_fraction", "{\"value\":0.4}", mpu_clamp_fraction(0.4));
    emit_num(2, "clamp_fraction", "{\"value\":-1}", mpu_clamp_fraction(-1.0));
    emit_num(3, "clamp_fraction", "{\"value\":2}", mpu_clamp_fraction(2.0));
    emit_num(4, "clamp_fraction", "{\"value\":0}", mpu_clamp_fraction(0.0));
    emit_num(5, "clamp_fraction", "{\"value\":1}", mpu_clamp_fraction(1.0));

    /* gauge_style */
    emit_str(6, "gauge_style", "{\"left\":0.9}", mpu_gauge_style(0.9));
    emit_str(7, "gauge_style", "{\"left\":0.5}", mpu_gauge_style(0.5));
    emit_str(8, "gauge_style", "{\"left\":0.19}", mpu_gauge_style(0.19));
    emit_str(9, "gauge_style", "{\"left\":0.0}", mpu_gauge_style(0.0));
    emit_str(10, "gauge_style", "{\"left\":-3}", mpu_gauge_style(-3.0));
    emit_str(11, "gauge_style", "{\"left\":4}", mpu_gauge_style(4.0));

    /* filled_cells */
    emit_int(12, "filled_cells", "{\"fraction\":0.0,\"width\":10}",
             mpu_filled_cells(0.0, 10));
    emit_int(13, "filled_cells", "{\"fraction\":0.04,\"width\":10}",
             mpu_filled_cells(0.04, 10));
    emit_int(14, "filled_cells", "{\"fraction\":0.96,\"width\":10}",
             mpu_filled_cells(0.96, 10));
    emit_int(15, "filled_cells", "{\"fraction\":1.0,\"width\":10}",
             mpu_filled_cells(1.0, 10));
    emit_int(16, "filled_cells", "{\"fraction\":0.55,\"width\":10}",
             mpu_filled_cells(0.55, 10));
    emit_int(17, "filled_cells", "{\"fraction\":0.5,\"width\":3}",
             mpu_filled_cells(0.5, 3));

    /* render_bar */
    {
        char b[64];
        mpu_render_bar(0.0, 10, b, sizeof(b));
        emit_str(18, "render_bar", "{\"fraction\":0.0,\"width\":10}", b);
        mpu_render_bar(0.5, 10, b, sizeof(b));
        emit_str(19, "render_bar", "{\"fraction\":0.5,\"width\":10}", b);
        mpu_render_bar(1.0, 10, b, sizeof(b));
        emit_str(20, "render_bar", "{\"fraction\":1.0,\"width\":10}", b);
        mpu_render_bar(0.04, 10, b, sizeof(b));
        emit_str(21, "render_bar", "{\"fraction\":0.04,\"width\":10}", b);
        mpu_render_bar(0.3, 4, b, sizeof(b));
        emit_str(22, "render_bar", "{\"fraction\":0.3,\"width\":4}", b);
    }

    /* format_percent_used */
    {
        char b[64];
        mpu_format_percent_used(0.0, b, sizeof(b));
        emit_str(23, "format_percent_used", "{\"used\":0.0}", b);
        mpu_format_percent_used(0.005, b, sizeof(b));
        emit_str(24, "format_percent_used", "{\"used\":0.005}", b);
        mpu_format_percent_used(0.02, b, sizeof(b));
        emit_str(25, "format_percent_used", "{\"used\":0.02}", b);
        mpu_format_percent_used(0.5, b, sizeof(b));
        emit_str(26, "format_percent_used", "{\"used\":0.5}", b);
        mpu_format_percent_used(1.0, b, sizeof(b));
        emit_str(27, "format_percent_used", "{\"used\":1.0}", b);
        mpu_format_percent_used(0.995, b, sizeof(b));
        emit_str(28, "format_percent_used", "{\"used\":0.995}", b);
    }

    /* compact_duration / format_resets_in */
    {
        char b[64];
        mpu_compact_duration(P("4000000"), P("0"), b, sizeof(b));
        emit_str(29, "compact_duration", "{\"resets\":4000000,\"now\":0}", b);
        mpu_compact_duration(P("3600000"), P("0"), b, sizeof(b));
        emit_str(30, "compact_duration", "{\"resets\":3600000,\"now\":0}", b);
        mpu_compact_duration(P("60000"), P("0"), b, sizeof(b));
        emit_str(31, "compact_duration", "{\"resets\":60000,\"now\":0}", b);
        mpu_compact_duration(P("59000"), P("0"), b, sizeof(b));
        emit_str(32, "compact_duration", "{\"resets\":59000,\"now\":0}", b);
        mpu_compact_duration(P("90000000"), P("0"), b, sizeof(b));
        emit_str(33, "compact_duration", "{\"resets\":90000000,\"now\":0}", b);
        mpu_compact_duration(P("null"), P("0"), b, sizeof(b));
        emit_str(34, "compact_duration", "{\"resets\":null,\"now\":0}", b);
        mpu_format_resets_in(P("3600000"), P("0"), b, sizeof(b));
        emit_str(35, "format_resets_in", "{\"resets\":3600000,\"now\":0}", b);
        mpu_format_resets_in(P("1000"), P("0"), b, sizeof(b));
        emit_str(36, "format_resets_in", "{\"resets\":1000,\"now\":0}", b);
    }

    /* split_recommended / featured_first */
    {
        json_t *rest = NULL;
        json_t *ids = P("[\"a\",\"b\",\"c\",\"d\"]");
        json_t *feat = P("[\"c\",\"a\"]");
        json_t *head = mpu_split_recommended(ids, feat, &rest);
        json_t *both = json_new_object();
        json_object_set(both, "head", head);
        json_object_set(both, "rest", rest);
        emit(37, "split_recommended",
             "{\"ids\":[\"a\",\"b\",\"c\",\"d\"],\"featured\":[\"c\",\"a\"]}", both);

        json_t *ids2 = P("[\"a\",\"b\"]");
        json_t *feat2 = P("[\"x\",\"y\"]");
        json_t *h2 = mpu_split_recommended(ids2, feat2, &rest);
        json_t *both2 = json_new_object();
        json_object_set(both2, "head", h2);
        json_object_set(both2, "rest", rest);
        emit(38, "split_recommended",
             "{\"ids\":[\"a\",\"b\"],\"featured\":[\"x\",\"y\"]}", both2);

        json_t *ids3 = P("[\"a\",\"b\"]");
        json_t *feat3 = P("[\"b\",\"b\",\"a\"]");
        json_t *h3 = mpu_split_recommended(ids3, feat3, &rest);
        json_t *both3 = json_new_object();
        json_object_set(both3, "head", h3);
        json_object_set(both3, "rest", rest);
        emit(39, "split_recommended",
             "{\"ids\":[\"a\",\"b\"],\"featured\":[\"b\",\"b\",\"a\"]}", both3);

        emit(40, "featured_first",
             "{\"ids\":[\"a\",\"b\",\"c\"],\"featured\":[\"c\",\"a\"]}",
             mpu_featured_first(P("[\"a\",\"b\",\"c\"]"), P("[\"c\",\"a\"]")));
        emit(41, "featured_first",
             "{\"ids\":[\"a\",\"b\"],\"featured\":[]}",
             mpu_featured_first(P("[\"a\",\"b\"]"), P("[]")));
    }

    /* credits_pay */
    emit_int(42, "credits_pay",
             "{\"info\":{\"paid_service_access_info\":{\"purchased_credits_remaining\":5}},"
             "\"window\":{\"keep_going_on_credits\":true}}",
             mpu_credits_pay(P("{\"paid_service_access_info\":{"
                               "\"purchased_credits_remaining\":5}}"),
                             P("{\"keep_going_on_credits\":true}")));
    emit_int(43, "credits_pay",
             "{\"info\":{\"paid_service_access_info\":{\"purchased_credits_remaining\":5}},"
             "\"window\":{\"keep_going_on_credits\":false}}",
             mpu_credits_pay(P("{\"paid_service_access_info\":{"
                               "\"purchased_credits_remaining\":5}}"),
                             P("{\"keep_going_on_credits\":false}")));
    emit_int(44, "credits_pay",
             "{\"info\":{},\"window\":{}}",
             mpu_credits_pay(P("{}"), P("{}")));
    emit_int(45, "credits_pay",
             "{\"info\":{\"paid_service_access_info\":{"
             "\"purchased_credits_remaining\":true}},\"window\":{}}",
             mpu_credits_pay(P("{\"paid_service_access_info\":{"
                               "\"purchased_credits_remaining\":true}}"),
                             P("{}")));

    /* is_free_model */
    emit_int(46, "is_free_model", "{\"row\":{\"prompt\":\"0\",\"completion\":\"0\"}}",
             mpu_is_free_model(P("{\"prompt\":\"0\",\"completion\":\"0\"}")));
    emit_int(47, "is_free_model", "{\"row\":{\"prompt\":\"1.5\",\"completion\":\"0\"}}",
             mpu_is_free_model(P("{\"prompt\":\"1.5\",\"completion\":\"0\"}")));
    emit_int(48, "is_free_model", "{\"row\":null}",
             mpu_is_free_model(P("null")));
    emit_int(49, "is_free_model", "{\"row\":{}}",
             mpu_is_free_model(P("{}")));

    /* capitalize */
    {
        char b[64];
        mpu_capitalize("hello world", b, sizeof(b));
        emit_str(50, "capitalize", "{\"text\":\"hello world\"}", b);
        mpu_capitalize("", b, sizeof(b));
        emit_str(51, "capitalize", "{\"text\":\"\"}", b);
        mpu_capitalize("a", b, sizeof(b));
        emit_str(52, "capitalize", "{\"text\":\"a\"}", b);
    }

    /* obj_get */
    emit(53, "obj_get", "{\"obj\":{\"k\":\"v\"},\"name\":\"k\",\"default\":null}",
         mpu_obj_get(P("{\"k\":\"v\"}"), "k", NULL));
    emit(54, "obj_get", "{\"obj\":{\"k\":\"v\"},\"name\":\"z\",\"default\":\"d\"}",
         mpu_obj_get(P("{\"k\":\"v\"}"), "z", P("\"d\"")));
    emit(55, "obj_get", "{\"obj\":null,\"name\":\"k\",\"default\":1}",
         mpu_obj_get(P("null"), "k", P("1")));
    return 0;
}
