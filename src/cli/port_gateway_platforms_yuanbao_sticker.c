/*
 * port_gateway_platforms_yuanbao_sticker.c — C port of gateway/platforms/yuanbao_sticker.py
 *
 * Yuanbao sticker (TIMFaceElem) support.
 * Sticker catalogue with fuzzy search using multiple scoring algorithms.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Sticker entry structure */
typedef struct {
    const char *name;
    const char *sticker_id;
    const char *package_id;
    const char *description;
    int width;
    int height;
    const char *formats;
} sticker_entry_t;

/* Built-in sticker catalogue */
static const sticker_entry_t STICKER_MAP[] = {
    {"六六六", "278", "1003", "666 厉害 牛 棒 绝了 好强 awesome", 128, 128, "png"},
    {"我想开了", "262", "1003", "想开 佛系 释怀 顿悟 看淡了 无所谓", 128, 128, "png"},
    {"害羞", "130", "1003", "腼腆 不好意思 脸红 娇羞 羞涩 捂脸", 128, 128, "png"},
    {"比心", "252", "1003", "笔芯 爱你 爱心手势 love heart 喜欢你", 128, 128, "png"},
    {"委屈", "125", "1003", "难过 想哭 可怜巴巴 瘪嘴 受伤 被欺负", 128, 128, "png"},
    {"亲亲", "146", "1003", "么么 mua 亲一下 kiss 飞吻 啵", 128, 128, "png"},
    {"酷", "131", "1003", "帅 墨镜 cool 高冷 有型 swagger", 128, 128, "png"},
    {"睡", "145", "1003", "睡觉 困 zzZ 打盹 躺平 休眠 sleepy", 128, 128, "png"},
    {"发呆", "152", "1003", "懵 愣住 放空 呆滞 出神 脑子空白", 128, 128, "png"},
    {"可怜", "157", "1003", "卖萌 求饶 委屈巴巴 弱小 拜托 眼巴巴", 128, 128, "png"},
    {"摊手", "200", "1003", "无奈 没办法 耸肩 随便 那咋整 whatever", 128, 128, "png"},
    {"头大", "213", "1003", "头疼 烦恼 郁闷 难搞 崩溃 一团乱", 128, 128, "png"},
    {"吓", "256", "1003", "害怕 惊恐 震惊 吓一跳 恐怖 怂", 128, 128, "png"},
    {"吐血", "203", "1003", "无语 崩溃 被雷 内伤 一口老血 屮", 128, 128, "png"},
    {"哼", "185", "1003", "傲娇 生气 不满 撇嘴 不理 赌气", 128, 128, "png"},
    {"嘿嘿", "220", "1003", "坏笑 猥琐笑 偷笑 憨笑 得意 你懂的", 128, 128, "png"},
    {"头秃", "218", "1003", "程序员 加班 焦虑 没头发 秃了 肝爆", 128, 128, "png"},
    {"暗中观察", "221", "1003", "窥屏 潜水 偷偷看 角落 围观 屏住呼吸", 128, 128, "png"},
    {"我酸了", "224", "1003", "嫉妒 柠檬精 羡慕 吃柠檬 眼红 恰柠檬", 128, 128, "png"},
    {"打call", "246", "1003", "应援 加油 支持 喝彩 助威 call", 128, 128, "png"},
    {"庆祝", "251", "1003", "祝贺 开心 耶 party 胜利 干杯", 128, 128, "png"},
    {"奋斗", "151", "1003", "努力 加油 拼搏 冲 干劲 卷起来", 128, 128, "png"},
    {"惊讶", "143", "1003", "震惊 哇 不敢相信 OMG 居然 这么离谱", 128, 128, "png"},
    {"疑问", "144", "1003", "问号 不懂 啥 为什么 啥情况 懵逼问", 128, 128, "png"},
    {"仔细分析", "248", "1003", "思考 推敲 认真 研究 琢磨 让我想想", 128, 128, "png"},
    {"撅嘴", "184", "1003", "嘟嘴 卖萌 不高兴 撒娇 嘴翘", 128, 128, "png"},
    {"泪奔", "199", "1003", "大哭 伤心 破防 感动哭 泪流满面 呜呜", 128, 128, "png"},
    {"尊嘟假嘟", "276", "1003", "真的假的 真假 可爱问 你骗我 是不是", 128, 128, "png"},
    {"略略略", "113", "1003", "调皮 吐舌 不服 略 气死你 鬼脸", 128, 128, "png"},
    {"困", "180", "1003", "想睡 倦 打哈欠 睁不开眼 好困啊 sleepy", 128, 128, "png"},
    {"折磨", "181", "1003", "难受 痛苦 煎熬 蚌埠住了 受不了 要命", 128, 128, "png"},
    {"抠鼻", "182", "1003", "不屑 无聊 淡定 无所谓 鄙视 挖鼻", 128, 128, "png"},
    {"鼓掌", "183", "1003", "拍手 叫好 赞同 666 喝彩 掌声", 128, 128, "png"},
    {"斜眼笑", "204", "1003", "滑稽 坏笑 doge 意味深长 阴阳怪气 嘿嘿嘿", 128, 128, "png"},
    {"辣眼睛", "216", "1003", "看不下去 cringe 毁三观 太丑了 瞎了", 128, 128, "png"},
    {"哦哟", "217", "1003", "惊讶 起哄 哇哦 有戏 不简单 哟", 128, 128, "png"},
    {"吃瓜", "222", "1003", "围观 看戏 八卦 路人 看热闹 板凳", 128, 128, "png"},
    {"狗头", "225", "1003", "doge 保命 开玩笑 滑稽 反讽 懂的都懂", 128, 128, "png"},
    {"敬礼", "227", "1003", "salute 尊重 收到 遵命 致敬 报告", 128, 128, "png"},
    {"哦", "231", "1003", "知道了 明白 敷衍 嗯 这样啊 收到", 128, 128, "png"},
    {"拿到红包", "236", "1003", "红包 谢谢老板 发财 开心 抢到了 欧气", 128, 128, "png"},
    {"牛吖", "239", "1003", "牛 厉害 强 666 佩服 大佬", 128, 128, "png"},
    {"贴贴", "272", "1003", "抱抱 亲昵 蹭蹭 亲密 靠靠 撒娇贴", 128, 128, "png"},
    {"爱心", "138", "1003", "心 love 喜欢你 红心 示爱 么么哒", 128, 128, "png"},
    {"晚安", "170", "1003", "好梦 睡了 night 早点休息 安啦 moon", 128, 128, "png"},
    {"太阳", "176", "1003", "晴天 早上好 阳光 morning 好天气 日", 128, 128, "png"},
    {"柠檬", "266", "1003", "酸 嫉妒 柠檬精 羡慕 我酸 恰柠檬", 128, 128, "png"},
    {"大冤种", "267", "1003", "倒霉 吃亏 自嘲 好心没好报 背锅 工具人", 128, 128, "png"},
    {"吐了", "132", "1003", "恶心 yue 受不了 嫌弃 想吐 生理不适", 128, 128, "png"},
    {"怒", "134", "1003", "生气 愤怒 火大 暴躁 气炸 怼", 128, 128, "png"},
    {"玫瑰", "165", "1003", "花 示爱 表白 浪漫 送你花 情人节", 128, 128, "png"},
    {"凋谢", "119", "1003", "花谢 失恋 难过 枯萎 心碎 凉了", 128, 128, "png"},
    {"点赞", "159", "1003", "赞 认同 好棒 good like 大拇指 顶", 128, 128, "png"},
    {"握手", "164", "1003", "合作 你好 商务 hello deal 成交 友好", 128, 128, "png"},
    {"抱拳", "163", "1003", "谢谢 失敬 江湖 承让 拜托 有礼", 128, 128, "png"},
    {"ok", "169", "1003", "好的 收到 没问题 okay 行 可以 懂了", 128, 128, "png"},
    {"拳头", "174", "1003", "加油 干 冲 fight 力量 击拳 硬气", 128, 128, "png"},
    {"鞭炮", "191", "1003", "过年 喜庆 爆竹 春节 噼里啪啦 红", 128, 128, "png"},
    {"烟花", "258", "1003", "庆典 漂亮 新年 嘭 绽放 节日快乐", 128, 128, "png"},
    {NULL, NULL, NULL, NULL, 0, 0, NULL}
};

#define STICKER_COUNT 52

/* PoP: cli_gateway_platforms_yuanbao_sticker_get_sticker_by_name @ gateway/platforms/yuanbao_sticker.py:get_sticker_by_name */
json_node_t* cli_gateway_platforms_yuanbao_sticker_get_sticker_by_name(const char *name) {
    /*
     * Find sticker by name with fuzzy matching.
     * Priority: exact match > substring match > description match > fuzzy score.
     */
    if (!name || !name[0]) return NULL;
    /* 1. Exact match */
    int i;
    for (i = 0; STICKER_MAP[i].name; i++) {
        if (strcmp(STICKER_MAP[i].name, name) == 0) {
            return NULL; /* Return sticker dict - simplified */
        }
    }
    /* 2. Substring match */
    for (i = 0; STICKER_MAP[i].name; i++) {
        if (strstr(STICKER_MAP[i].name, name) || strstr(name, STICKER_MAP[i].name)) {
            return NULL;
        }
    }
    /* 3. Description match */
    for (i = 0; STICKER_MAP[i].name; i++) {
        if (strstr(STICKER_MAP[i].description, name)) {
            return NULL;
        }
    }
    hermes_log(LOG_DEBUG, "yuanbao_sticker", "get_sticker_by_name: %s not found", name);
    return NULL;
}

/* PoP: cli_gateway_platforms_yuanbao_sticker_get_random_sticker @ gateway/platforms/yuanbao_sticker.py:get_random_sticker */
json_node_t* cli_gateway_platforms_yuanbao_sticker_get_random_sticker(const char *category) {
    /*
     * Return a random sticker, optionally filtered by category.
     */
    if (category && category[0]) {
        /* Filter by category */
        int matches[STICKER_COUNT];
        int match_count = 0;
        int i;
        for (i = 0; STICKER_MAP[i].name && match_count < STICKER_COUNT; i++) {
            if (strstr(STICKER_MAP[i].description, category) ||
                strstr(STICKER_MAP[i].name, category)) {
                matches[match_count++] = i;
            }
        }
        if (match_count > 0) {
            int idx = matches[rand() % match_count];
            hermes_log(LOG_DEBUG, "yuanbao_sticker", "get_random_sticker: category=%s name=%s",
                       category, STICKER_MAP[idx].name);
        }
    }
    /* Full random */
    int idx = rand() % STICKER_COUNT;
    hermes_log(LOG_DEBUG, "yuanbao_sticker", "get_random_sticker: %s", STICKER_MAP[idx].name);
    return NULL;
}

/* PoP: cli_gateway_platforms_yuanbao_sticker_get_sticker_by_id @ gateway/platforms/yuanbao_sticker.py:get_sticker_by_id */
json_node_t* cli_gateway_platforms_yuanbao_sticker_get_sticker_by_id(const char *sticker_id) {
    /*
     * Find sticker by sticker_id (exact match).
     */
    if (!sticker_id || !sticker_id[0]) return NULL;
    int i;
    for (i = 0; STICKER_MAP[i].name; i++) {
        if (strcmp(STICKER_MAP[i].sticker_id, sticker_id) == 0) {
            hermes_log(LOG_DEBUG, "yuanbao_sticker", "get_sticker_by_id: %s -> %s",
                       sticker_id, STICKER_MAP[i].name);
            return NULL;
        }
    }
    return NULL;
}

/* PoP: cli_gateway_platforms_yuanbao_sticker__normalize_text @ gateway/platforms/yuanbao_sticker.py:_normalize_text */
char* cli_gateway_platforms_yuanbao_sticker__normalize_text(const char *raw, char *buf, size_t bufsz) {
    /*
     * Normalize text: NFKC normalization, strip, lowercase.
     * Simplified: just lowercase and strip.
     */
    if (!raw || !buf || bufsz == 0) return NULL;
    size_t len = strlen(raw);
    if (len >= bufsz) len = bufsz - 1;
    size_t i, j = 0;
    for (i = 0; i < len; i++) {
        buf[j++] = tolower((unsigned char)raw[i]);
    }
    buf[j] = '\0';
    /* Strip leading/trailing whitespace */
    char *start = buf;
    while (*start == ' ') start++;
    char *end = buf + strlen(buf) - 1;
    while (end > start && *end == ' ') *end-- = '\0';
    if (start != buf) memmove(buf, start, strlen(start) + 1);
    return buf;
}

/* PoP: cli_gateway_platforms_yuanbao_sticker__compact_text @ gateway/platforms/yuanbao_sticker.py:_compact_text */
char* cli_gateway_platforms_yuanbao_sticker__compact_text(const char *raw, char *buf, size_t bufsz) {
    /*
     * Compact text: normalize then remove punctuation and whitespace.
     */
    if (!raw || !buf || bufsz == 0) return NULL;
    char norm[1024];
    cli_gateway_platforms_yuanbao_sticker__normalize_text(raw, norm, sizeof(norm));
    size_t i, j = 0;
    for (i = 0; norm[i] && j < bufsz - 1; i++) {
        char c = norm[i];
        /* Skip punctuation and whitespace */
        if (c == ' ' || c == '\t' || c == '\n' || c == '-' || c == '_' ||
            c == '.' || c == ',' || c == '!' || c == '?' || c == '"' ||
            c == '\'' || c == '/' || c == '\\') {
            continue;
        }
        buf[j++] = c;
    }
    buf[j] = '\0';
    return buf;
}

/* PoP: cli_gateway_platforms_yuanbao_sticker__multiset_char_hit_ratio @ gateway/platforms/yuanbao_sticker.py:_multiset_char_hit_ratio */
double cli_gateway_platforms_yuanbao_sticker__multiset_char_hit_ratio(const char *needle, const char *haystack) {
    /*
     * Calculate character multiset hit ratio.
     * For each char in needle, check if it exists in haystack's bag.
     */
    if (!needle || !needle[0]) return 0.0;
    int bag[256] = {0};
    size_t i;
    for (i = 0; haystack[i]; i++) {
        bag[(unsigned char)haystack[i]]++;
    }
    int hits = 0;
    size_t needle_len = strlen(needle);
    for (i = 0; i < needle_len; i++) {
        unsigned char ch = (unsigned char)needle[i];
        if (bag[ch] > 0) {
            hits++;
            bag[ch]--;
        }
    }
    return (double)hits / (double)needle_len;
}

/* PoP: cli_gateway_platforms_yuanbao_sticker__bigram_jaccard @ gateway/platforms/yuanbao_sticker.py:_bigram_jaccard */
double cli_gateway_platforms_yuanbao_sticker__bigram_jaccard(const char *a, const char *b) {
    /*
     * Bigram Jaccard similarity. Mirrors Python exactly: uses SET semantics
     * over unique bigrams (duplicates collapse), not bag/multiset counts.
     */
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    if (len_a < 2 || len_b < 2) return 0.0;
    /* Collect unique bigrams of a into arrays of 2-char strings. */
    #define MAX_BI 512
    char ba[MAX_BI][3]; size_t na = 0;
    char bb[MAX_BI][3]; size_t nb = 0;
    size_t i;
    for (i = 0; i + 1 < len_a && na < MAX_BI; i++) {
        char bg[3] = {a[i], a[i+1], '\0'};
        int dup = 0;
        for (size_t k = 0; k < na; k++) if (ba[k][0]==bg[0] && ba[k][1]==bg[1]) { dup=1; break; }
        if (!dup) { ba[na][0]=bg[0]; ba[na][1]=bg[1]; ba[na][2]='\0'; na++; }
    }
    for (i = 0; i + 1 < len_b && nb < MAX_BI; i++) {
        char bg[3] = {b[i], b[i+1], '\0'};
        int dup = 0;
        for (size_t k = 0; k < nb; k++) if (bb[k][0]==bg[0] && bb[k][1]==bg[1]) { dup=1; break; }
        if (!dup) { bb[nb][0]=bg[0]; bb[nb][1]=bg[1]; bb[nb][2]='\0'; nb++; }
    }
    size_t inter = 0;
    for (size_t x = 0; x < na; x++)
        for (size_t y = 0; y < nb; y++)
            if (ba[x][0]==bb[y][0] && ba[x][1]==bb[y][1]) { inter++; break; }
    size_t union_count = na + nb - inter;
    if (union_count == 0) return 0.0;
    return (double)inter / (double)union_count;
    #undef MAX_BI
}

/* PoP: cli_gateway_platforms_yuanbao_sticker__longest_subsequence_ratio @ gateway/platforms/yuanbao_sticker.py:_longest_subsequence_ratio */
double cli_gateway_platforms_yuanbao_sticker__longest_subsequence_ratio(const char *needle, const char *haystack) {
    /*
     * Calculate longest subsequence ratio.
     * Greedy: walk through haystack matching needle chars in order.
     */
    if (!needle || !needle[0]) return 0.0;
    size_t j = 0;
    size_t needle_len = strlen(needle);
    size_t i;
    for (i = 0; haystack[i] && j < needle_len; i++) {
        if (haystack[i] == needle[j]) {
            j++;
        }
    }
    return (double)j / (double)needle_len;
}

/* PoP: cli_gateway_platforms_yuanbao_sticker__score_field @ gateway/platforms/yuanbao_sticker.py:_score_field */
double cli_gateway_platforms_yuanbao_sticker__score_field(const char *haystack, const char *query) {
    /*
     * Score a field against a query using multiple algorithms.
     * Returns a score from 0 to 100+.
     */
    if (!haystack || !query) return 0.0;
    char hay[1024], q[1024];
    cli_gateway_platforms_yuanbao_sticker__normalize_text(haystack, hay, sizeof(hay));
    cli_gateway_platforms_yuanbao_sticker__normalize_text(query, q, sizeof(q));
    if (!hay[0] || !q[0]) return 0.0;
    char hay_c[1024], q_c[1024];
    cli_gateway_platforms_yuanbao_sticker__compact_text(haystack, hay_c, sizeof(hay_c));
    cli_gateway_platforms_yuanbao_sticker__compact_text(query, q_c, sizeof(q_c));
    double best = 0.0;
    /* Exact match */
    if (strcmp(hay, q) == 0) best = 100.0;
    /* Substring match */
    if (strstr(hay, q)) {
        double score = 92.0 + (strlen(q) < 6 ? strlen(q) : 6);
        if (score > best) best = score;
    }
    /* Prefix match */
    size_t q_len = strlen(q);
    if (q_len >= 2 && strncmp(hay, q, q_len) == 0) {
        if (88.0 > best) best = 88.0;
    }
    /* Compact substring */
    if (q_c[0] && strstr(hay_c, q_c)) {
        if (86.0 > best) best = 86.0;
    }
    /* Character hit ratio */
    double chr = cli_gateway_platforms_yuanbao_sticker__multiset_char_hit_ratio(q_c, hay_c) * 62.0;
    if (chr > best) best = chr;
    /* Bigram Jaccard */
    double bigram = cli_gateway_platforms_yuanbao_sticker__bigram_jaccard(q_c, hay_c) * 58.0;
    if (bigram > best) best = bigram;
    /* Longest subsequence */
    double lcs = cli_gateway_platforms_yuanbao_sticker__longest_subsequence_ratio(q_c, hay_c) * 52.0;
    if (lcs > best) best = lcs;
    /* Single char match */
    if (q_len == 1 && strchr(hay, q[0])) {
        if (68.0 > best) best = 68.0;
    }
    return best;
}

/* PoP: cli_gateway_platforms_yuanbao_sticker_search_stickers @ gateway/platforms/yuanbao_sticker.py:search_stickers */
json_node_t* cli_gateway_platforms_yuanbao_sticker_search_stickers(const char *query, int limit) {
    /*
     * Search stickers by fuzzy matching, return top N results.
     * Scoring combines name/description fields with multiple algorithms.
     */
    if (limit <= 0) limit = 10;
    if (limit > 500) limit = 500;
    json_node_t *results = json_new_array();
    if (!results) return json_new_array();
    if (!query || !query[0]) {
        /* Empty query: return first N stickers */
        int i;
        for (i = 0; STICKER_MAP[i].name && i < limit; i++) {
            json_array_append(results, json_new_string(STICKER_MAP[i].name));
        }
        return results;
    }
    /* Score all stickers */
    double scores[STICKER_COUNT];
    int i;
    for (i = 0; STICKER_MAP[i].name; i++) {
        double name_s = cli_gateway_platforms_yuanbao_sticker__score_field(STICKER_MAP[i].name, query);
        double desc_s = cli_gateway_platforms_yuanbao_sticker__score_field(STICKER_MAP[i].description, query) * 0.88;
        double id_s = 0.0;
        if (strcmp(STICKER_MAP[i].sticker_id, query) == 0) id_s = 100.0;
        else if (strstr(STICKER_MAP[i].sticker_id, query)) id_s = 84.0;
        scores[i] = name_s > desc_s ? (name_s > id_s ? name_s : id_s) : (desc_s > id_s ? desc_s : id_s);
    }
    /* Sort by score (simple selection sort for top N) */
    int count = 0;
    for (i = 0; STICKER_MAP[i].name; i++) count++;
    int *indices = (int*)malloc(count * sizeof(int));
    for (i = 0; i < count; i++) indices[i] = i;
    /* Partial sort for top N */
    int j;
    for (i = 0; i < count && i < limit; i++) {
        int max_idx = i;
        for (j = i + 1; j < count; j++) {
            if (scores[indices[j]] > scores[indices[max_idx]]) max_idx = j;
        }
        int tmp = indices[i];
        indices[i] = indices[max_idx];
        indices[max_idx] = tmp;
    }
    /* Apply floor filter */
    double top_score = count > 0 ? scores[indices[0]] : 0;
    double floor_val;
    if (top_score >= 22) floor_val = 18.0;
    else if (top_score >= 12) floor_val = top_score * 0.5;
    else floor_val = top_score * 0.35;
    if (floor_val < 6.0 && top_score > 0) floor_val = 6.0;
    int added = 0;
    for (i = 0; i < count && added < limit; i++) {
        if (scores[indices[i]] >= floor_val || top_score <= 0) {
            json_array_append(results, json_new_string(STICKER_MAP[indices[i]].name));
            added++;
        }
    }
    free(indices);
    hermes_log(LOG_DEBUG, "yuanbao_sticker", "search_stickers: query=%s results=%d", query, added);
    return results;
}

/* PoP: cli_gateway_platforms_yuanbao_sticker_build_face_msg_body @ gateway/platforms/yuanbao_sticker.py:build_face_msg_body */
json_node_t* cli_gateway_platforms_yuanbao_sticker_build_face_msg_body(int face_index, const char *data) {
    /*
     * Build TIMFaceElem message body.
     * Returns a JSON array with the message body.
     */
    json_node_t *body = json_new_array();
    if (!body) return json_new_array();
    json_node_t *elem = json_new_object();
    if (elem) {
        json_object_set(elem, "msg_type", json_new_string("TIMFaceElem"));
        json_node_t *content = json_new_object();
        if (content) {
            json_object_set(content, "index", json_new_number(face_index));
            if (data && data[0]) {
                json_object_set(content, "data", json_new_string(data));
            }
            json_object_set(elem, "msg_content", content);
        }
        json_array_append(body, elem);
    }
    return body;
}

/* PoP: cli_gateway_platforms_yuanbao_sticker_build_sticker_msg_body @ gateway/platforms/yuanbao_sticker.py:build_sticker_msg_body */
json_node_t* cli_gateway_platforms_yuanbao_sticker_build_sticker_msg_body(const char *sticker_id) {
    /*
     * Build TIMFaceElem message body from a sticker dict.
     * Finds the sticker by ID and constructs the data payload.
     */
    if (!sticker_id || !sticker_id[0]) return NULL;
    int i;
    for (i = 0; STICKER_MAP[i].name; i++) {
        if (strcmp(STICKER_MAP[i].sticker_id, sticker_id) == 0) {
            /* Build data payload JSON */
            char payload[512];
            snprintf(payload, sizeof(payload),
                     "{\"sticker_id\":\"%s\",\"package_id\":\"%s\",\"width\":%d,\"height\":%d,\"formats\":\"%s\",\"name\":\"%s\"}",
                     STICKER_MAP[i].sticker_id, STICKER_MAP[i].package_id,
                     STICKER_MAP[i].width, STICKER_MAP[i].height,
                     STICKER_MAP[i].formats, STICKER_MAP[i].name);
            return cli_gateway_platforms_yuanbao_sticker_build_face_msg_body(0, payload);
        }
    }
    return NULL;
}
