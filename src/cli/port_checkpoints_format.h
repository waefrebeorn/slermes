#ifndef CLI_CHECKPOINTS_FORMAT_H
#define CLI_CHECKPOINTS_FORMAT_H
void hermes_cli_checkpoints_fmt_bytes(long n, char *out, size_t outsz);
void hermes_cli_checkpoints_fmt_age(double ts, double now, char *out, size_t outsz);
#endif
