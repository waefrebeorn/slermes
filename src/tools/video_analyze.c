/*
 * video_analyze.c — Video analysis tool for Hermes C.
 * Extracts video metadata via ffprobe and optionally performs
 * deeper analysis via Python delegation (scene detection, OCR, etc.).
 */

/* PoP: video generation tool (port of tools/video_generation_tool) */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

/* Max file size for local video analysis (500 MB) */
#define VIDEO_MAX_BYTES (500LL * 1024 * 1024)

/* Schema */
static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"video_url\":{\"type\":\"string\",\"description\":\"Path or URL to video file\"},"
      "\"analysis\":{\"type\":\"string\",\"description\":\"Optional analysis type: 'metadata' (codec, resolution, duration, bitrate, framerate), 'scenes' (scene boundary detection), 'audio' (audio stream info). Default: metadata\",\"default\":\"metadata\",\"enum\":[\"metadata\",\"scenes\",\"audio\"]}"
    "},"
    "\"required\":[\"video_url\"]"
"}";

/* Run command and capture full output */
#ifdef _WIN32
#include "terminal_cmd.h"

/* Was popen() into a /tmp temp file: on Windows the interpreter is not there and /tmp does not exist,
 * so every probe returned NULL and the tool said "ffprobe returned no output" about a file it had
 * successfully stat'd. Use the native runner and hand back its stdout. */
static char *run_cmd_full(const char *fmt, ...) {
    char cmd[8192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);

    char *rj = run_command(cmd, 120);
    if (!rj) return NULL;
    json_t *j = json_parse(rj, NULL);
    const char *out = j ? json_get_str(j, "output", "") : "";
    char *res = (out && *out) ? strdup(out) : NULL;
    if (j) json_free(j);
    free(rj);
    return res;
}

/* ffprobe ships beside ffmpeg but is not on the server's PATH. Resolve it once, and let the caller
 * say "ffprobe is not installed" instead of "returned no output". */
static const char *probe_bin(void) {
    static char found[MAX_PATH];
    static int resolved = 0;
    if (!resolved) {
        resolved = 1;
        const char *env = getenv("HERMES_FFPROBE");
        if (env && *env) {
            snprintf(found, sizeof(found), "%s", env);
        } else if (SearchPathA(NULL, "ffprobe.exe", NULL, sizeof(found), found, NULL)) {
            /* found */
        } else {
            const char *lad = getenv("LOCALAPPDATA");
            if (lad && *lad) {
                snprintf(found, sizeof(found), "%s\\Microsoft\\WinGet\\Links\\ffprobe.exe", lad);
                if (GetFileAttributesA(found) == INVALID_FILE_ATTRIBUTES) found[0] = '\0';
            }
        }
    }
    return found[0] ? found : NULL;
}
#else
static char *run_cmd_full(const char *fmt, ...) {
    char cmd[8192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    char tmp[] = "/tmp/hermes_video_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd < 0) { pclose(fp); return NULL; }

    char buf[4096];
    size_t written = 0;
    ssize_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        ssize_t r = write(fd, buf, (size_t)n);
        if (r < 0) break;
        written += (size_t)r;
    }
    pclose(fp);

    char *out = malloc(written + 1);
    if (out) {
        lseek(fd, 0, SEEK_SET);
        ssize_t r = read(fd, out, written);
        if (r > 0) out[r] = '\0';
        else out[0] = '\0';
    }
    close(fd);
    unlink(tmp);
    return out;
}
static const char *probe_bin(void) { return "ffprobe"; }
#endif /* _WIN32 */

/* Check if URL has a known video extension */
static bool has_video_extension(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return false;
    ext++;
    const char *exts[] = {"mp4","avi","mov","mkv","webm","flv","wmv","m4v","mpg","mpeg","3gp","ogv", NULL};
    for (int i = 0; exts[i]; i++) {
        size_t elen = strlen(exts[i]);
        if (strncasecmp(ext, exts[i], elen) == 0 && strlen(ext) == elen)
            return true;
    }
    return false;
}

char *video_analyze_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *video_url = json_object_get_string(args, "video_url", NULL);
    const char *analysis = json_object_get_string(args, "analysis", "metadata");

    json_node_t *result = json_new_object();

    if (!video_url) {
        json_object_set(result, "error", json_new_string("Missing video_url"));
    } else {
        /* A file:// URL is a local file: it used to fall into the remote branch's stat-less path and be
         * answered "File not found" while sitting right there. `norm` must outlive the branch -- the
         * first version declared it inside the if and left video_url dangling, which killed the server. */
        char norm[MAX_PATH * 2] = "";
        if (strncmp(video_url, "file://", 7) == 0) {
            const char *p = video_url + 7;
            if (strncmp(p, "localhost/", 10) == 0) p += 9;
            while (*p == '/') p++;                    /* file:///C:/... -> C:/... */
            snprintf(norm, sizeof(norm), "%s", p);
#ifdef _WIN32
            for (char *q = norm; *q; q++) if (*q == '/') *q = '\\';
#endif
            video_url = norm;
        }
        bool is_local = (strncmp(video_url, "http://", 7) != 0 &&
                         strncmp(video_url, "https://", 8) != 0 &&
                         strncmp(video_url, "data:", 5) != 0);

        if (is_local) {
            struct stat st;
            if (stat(video_url, &st) != 0) {
                json_object_set(result, "error", json_new_string("File not found"));
            } else if (!has_video_extension(video_url)) {
                json_object_set(result, "error", json_new_string("Not a recognized video format"));
            } else if (st.st_size > VIDEO_MAX_BYTES) {
                json_object_set(result, "error", json_new_string("File too large (>500 MB)"));
            } else {
                json_object_set(result, "video_url", json_new_string(video_url));
                json_object_set(result, "file_size", json_new_number((double)st.st_size));

                /* Metadata analysis via ffprobe */
                if (strcmp(analysis, "metadata") == 0 || strcmp(analysis, "audio") == 0) {
                    const char *ffp = probe_bin();
                    char *ffprobe_out = ffp ? run_cmd_full(
                        "\"%s\" -v quiet -print_format json -show_format -show_streams \"%s\" 2>/dev/null",
                        ffp, video_url) : NULL;
                    if (ffprobe_out && strlen(ffprobe_out) > 0) {
                        char *parse_err = NULL;
                        json_node_t *probe = json_parse(ffprobe_out, &parse_err);
                        if (probe) {
                            /* Extract video stream info */
                            json_node_t *streams = json_object_get(probe, "streams");
                            if (streams) {
                                size_t n = json_len(streams);
                                json_node_t *video_streams = json_new_array();
                                json_node_t *audio_streams = json_new_array();
                                for (size_t i = 0; i < n; i++) {
                                    json_node_t *s = json_get(streams, i);
                                    const char *codec_type = json_get_str(s, "codec_type", "unknown");
                                    bool want_video = (strcmp(codec_type, "video") == 0);
                                    bool want_audio = (strcmp(codec_type, "audio") == 0);
                                    if (want_video || want_audio) {
                                        /* These arrays used to take the stream nodes themselves, and
                                         * then json_free(probe) released them: the reply carried freed
                                         * nodes and serialising it killed the server (WinError 10054).
                                         * Copy through the wire form so the reply owns its subtrees. */
                                        char *wire = json_serialize(s);
                                        json_node_t *copy = wire ? json_parse(wire, NULL) : NULL;
                                        free(wire);
                                        if (copy) json_append(want_video ? video_streams : audio_streams, copy);
                                    }
                                }
                                if (json_len(video_streams) > 0)
                                    json_object_set(result, "video_streams", video_streams);
                                else
                                    json_free(video_streams);
                                if (json_len(audio_streams) > 0)
                                    json_object_set(result, "audio_streams", audio_streams);
                                else
                                    json_free(audio_streams);
                            }

                            /* Format info */
                            json_node_t *fmt = json_object_get(probe, "format");
                            if (fmt) {
                                const char *fmt_name = json_get_str(fmt, "format_name", NULL);
                                const char *duration = json_get_str(fmt, "duration", NULL);
                                const char *bitrate = json_get_str(fmt, "bit_rate", NULL);
                                const char *size_str = json_get_str(fmt, "size", NULL);
                                if (fmt_name) json_object_set(result, "format", json_new_string(fmt_name));
                                if (duration) json_object_set(result, "duration_sec", json_new_string(duration));
                                if (bitrate) json_object_set(result, "bitrate_bps", json_new_string(bitrate));
                                if (size_str) json_object_set(result, "container_size", json_new_string(size_str));
                            }
                            json_free(probe);
                        }
                        free(parse_err);
                    } else {
                        json_object_set(result, "probe_warning", json_new_string(
                            probe_bin() ? "ffprobe returned no output"
                                        : "ffprobe is not installed on this machine (install ffmpeg, or set HERMES_FFPROBE)"));
                    }
                    free(ffprobe_out);
                }

                /* Audio-specific analysis */
                if (strcmp(analysis, "audio") == 0) {
                    /* ffprobe already handled above, we just add audio focus */
                    json_object_set(result, "analysis_type", json_new_string("audio"));
                }

                /* Scene detection via ffprobe scene detection */
                if (strcmp(analysis, "scenes") == 0) {
                    char *scenes = run_cmd_full(
                        "ffprobe -v quiet -show_entries "
                        "frame=pts_time:frame_tags=lavfi.scene_score "
                        "-f lavfi -i \"movie='%s',select=gt(scene\\\\,0.4)\" "
                        "2>/dev/null | grep -E 'pts_time|scene_score' | head -100",
                        video_url);
                    if (scenes && strlen(scenes) > 0) {
                        json_object_set(result, "scenes", json_new_string(scenes));
                    } else {
                        json_object_set(result, "scene_note", json_new_string("Scene detection requires ffmpeg with lavfi. No scene transitions found or filter unavailable."));
                    }
                    free(scenes);
                    json_object_set(result, "analysis_type", json_new_string("scenes"));
                }
            }
        } else {
            /* Remote URL — return basic info */
            json_object_set(result, "video_url", json_new_string(video_url));
            json_object_set(result, "note", json_new_string("Remote URL; metadata limited. Download locally for full analysis."));
        }
    }

    /* Upstream video_analyze returns {success, analysis} from a multimodal model; this port has no
     * vision call at all (grep: zero provider references in this file). Returning bare ffprobe metadata
     * reads like "analysed, found nothing", so say what actually happened. */
    if (!json_object_get(result, "error")) {
        const char *keys[] = {"OPENAI_API_KEY", "XAI_API_KEY", "ANTHROPIC_API_KEY",
                              "GOOGLE_API_KEY", "GEMINI_API_KEY", "VOICE_TOOLS_OPENAI_KEY", NULL};
        bool have_key = false;
        for (size_t i = 0; keys[i]; i++) {
            const char *v = getenv(keys[i]);
            if (v && *v) { have_key = true; break; }
        }
        json_object_set(result, "success", json_new_bool(false));
        json_object_set(result, "analysis", json_new_string(""));
        json_object_set(result, "error", json_new_string(have_key
            ? "Video analysis is not wired to a multimodal model in this port; only the ffprobe metadata "
              "above is available."
            : "No video analysis model configured: no vision provider key is set (OPENAI_API_KEY, "
              "XAI_API_KEY, ANTHROPIC_API_KEY, GOOGLE_API_KEY). Only the ffprobe metadata above is "
              "available."));
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_video_analyze(void) {
    registry_register("video_analyze",
        "Analyze a video file. Returns metadata (codec, resolution, duration, "
        "bitrate, framerate) via ffprobe and optionally performs scene detection "
        "or audio analysis. Supports local files and remote URLs.",
        SCHEMA, video_analyze_handler);
}
