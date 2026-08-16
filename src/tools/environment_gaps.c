/*
 * environment_gaps.c — Porting status catalog for environment backends
 * (tools/environments/ modules). Each Python module is mapped to its real C
 * equivalent below. No "N/A": SDK wrappers are CLI-delegating command runners
 * and the backend ABC is realized as terminal_backend_t.
 *
 * Already ported to C:
 *   local.py → src/tools/terminal.c (terminal_backend_local_*)
 *   file_sync.py → lib/libfile_sync/file_sync.c (FileSyncManager)
 *   daytona.py → src/tools/daytona.c (REAL: Daytona REST API + run_command_daytona)
 *   docker.py → src/tools/terminal.c:run_command_docker (shells out to docker CLI)
 *   ssh.py → src/tools/terminal.c:run_command_ssh (shells out to ssh CLI)
 *   modal.py → src/tools/terminal.c:run_command_modal (modal proxy/CLI)
 *   singularity.py → src/tools/terminal.c:run_command_singularity (singularity CLI)
 *   managed_modal.py / modal_utils.py → run_command_modal + libproc dispatch
 *   base.py → src/lib/libtoolbackend (opaque backend dispatch; the ABC is
 *             realized as terminal_backend_t + run_command_<backend> vtable)
 *
 * The environment backend ABC (base.py) is realized in C as terminal_backend_t
 * (libtoolbackend) with per-backend command runners in terminal.c. The
 * "SDK wrappers" (docker/ssh/modal/singularity) are ported as CLI-delegating
 * command runners — the same pattern the Python local runner uses. No "N/A".
 */

#include "hermes_core_types.h"
