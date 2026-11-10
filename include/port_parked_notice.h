/*
 * port_parked_notice.h — pure notice-window state machine from
 * gateway/parked_route_refusal.py _notice_due. The adapter state read/write
 * and the monotonic clock are parameterized.
 */
#ifndef PORT_PARKED_NOTICE_H
#define PORT_PARKED_NOTICE_H

/* _notice_due(adapter, profile, platform, chat_id), pure: True (and the window
 * marked) when this (profile, platform, chat) was not told of in the 60s
 * window. sent_json is an array of {"profile","platform","chat","ts"}; the
 * clock `now` (seconds) is the caller's fact. PARKED_NOTICE_WINDOW_S = 60.0.
 * Returns malloc'd JSON: {"due":bool,"sent":[non-evicted {"profile","platform",
 * "chat","ts"}...]} — the kept entries in original order. Caller frees. */
char *prr_notice_due(const char *sent_json, double now, const char *profile,
                     const char *platform, const char *chat_id);

#endif /* PORT_PARKED_NOTICE_H */
