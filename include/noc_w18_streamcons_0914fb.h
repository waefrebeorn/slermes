#ifndef NOC_W18_STREAMCONS_0914FB_H
#define NOC_W18_STREAMCONS_0914FB_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_streamcons_0914fb_probe_start @ gateway/stream_consumer_redact.py:_probe_start
 * Where to probe the trailing token from (byte index, -1 when none). */
long long w18_streamcons_0914fb_probe_start(const char *text, long long token_start);

/* PoP: w18_streamcons_0914fb_lookback_start @ gateway/stream_consumer_redact.py:_lookback_start
 * Where re-redaction restarts (byte index). */
long long w18_streamcons_0914fb_lookback_start(const char *text, long long upto);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_STREAMCONS_0914FB_H */
