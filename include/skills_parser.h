/*
 * skills_parser.h — SKILL.md YAML frontmatter parser header
 *
 * MIT License — Slermes Fork
 */
#ifndef SKILLS_PARSER_H
#define SKILLS_PARSER_H

typedef struct {
    char name[64];
    char description[512];
    char version[32];
    char author[64];
    char tags[16][64];
    int  tag_count;
    char dependencies[16][64];
    int  dep_count;
    char path[1024];
} skill_info_t;

/* Discover skills from ~/.slermes/skills/ and optional extra path */
int skills_discover(skill_info_t *skills, int max_skills, const char *extra_path);

/* Build JSON representation of skills into global buffer */
void skills_build_json(int count, skill_info_t *skills);

/* Return cached JSON buffer; rebuilds if invalidated */
int skills_get_json(char *buf, size_t bufsz);

/* Invalidate cache (e.g., after skill install/remove) */
void skills_invalidate_cache(void);

#endif /* SKILLS_PARSER_H */
