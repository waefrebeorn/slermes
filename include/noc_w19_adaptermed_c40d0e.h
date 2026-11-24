#ifndef NOC_W19_ADAPTERMED_C40D0E_H
#define NOC_W19_ADAPTERMED_C40D0E_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_adaptermed_c40d0e_ogg_opus_duration @ plugins/platforms/discord/adapter_media.py:_ogg_opus_duration
 * Playback seconds (last page granule minus OpusHead pre-skip, 48 kHz) or json_null() when the
 * bytes are not Ogg/Opus. */
struct json_t *w19_adaptermed_c40d0e_ogg_opus_duration(const unsigned char *data, unsigned long n);

/* PoP: w19_adaptermed_c40d0e_upload_limit @
 * plugins/platforms/discord/adapter_media.py:_discord_upload_limit_bytes
 * guild is null (DM/group DM) or {"filesize_limit": int}; the guild value is floored at 20 MiB. */
struct json_t *w19_adaptermed_c40d0e_upload_limit(const struct json_t *guild);

/* PoP: w19_adaptermed_c40d0e_oversized @
 * plugins/platforms/discord/adapter_media.py:_reject_oversized_upload
 * limit is the channel's guild filesize_limit; like _discord_upload_limit_bytes it is floored at
 * the 20 MiB platform default before the comparison.
 * {"rejected":bool,"error":string|null} - the preflight decision plus its message. */
struct json_t *w19_adaptermed_c40d0e_oversized(const char *filename, long long file_size,
                                               long long limit);

/* PoP: w19_adaptermed_c40d0e_reply_count @
 * plugins/platforms/discord/adapter_media.py:reply_message_count
 * {"count":n} for the 2,000-char split (chunking supplied by the caller's truncate seam). */
struct json_t *w19_adaptermed_c40d0e_reply_count(const char *content, long long max_len);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_ADAPTERMED_C40D0E_H */
