/*
 * daytona.c — Daytona cloud sandbox execution backend.
 *
 * Port of Python tools/environments/daytona.py (270 lines).
 * ALL methods are N/A — they wrap the Daytona Python SDK
 * (Daytona, CreateSandboxFromImageParams, DaytonaError, Resources,
 * SandboxState, FileUpload) which has no C equivalent.
 *
 * The C terminal.c supports backend selection (local, docker, ssh, modal,
 * singularity) natively. Daytona cloud sandbox execution requires the
 * full Daytona Python SDK and is not portable to embedded C.
 *
 * N/A: DaytonaEnvironment.__init__() — Daytona SDK constructor + sandbox
 *       create/resume logic using daytona.Daytona(), daytona.CreateSandboxFromImageParams,
 *       daytona.Resources, daytona.SandboxState
 * N/A: _daytona_upload() — sandbox.fs.upload_file() SDK call
 * N/A: _daytona_bulk_upload() — sandbox.fs.upload_files() with FileUpload SDK type
 * N/A: _daytona_bulk_download() — sandbox.fs.download_file() SDK call
 * N/A: _daytona_delete() — sandbox.process.exec() SDK call
 * N/A: _ensure_sandbox_ready() — sandbox.refresh_data() + sandbox.start() SDK calls
 * N/A: _before_execute() — calls _ensure_sandbox_ready() + FileSyncManager.sync()
 * N/A: _run_bash() — sandbox.process.exec() SDK call via _ThreadedProcessHandle
 * N/A: cleanup() — sandbox.stop() / daytona.delete() SDK calls + FileSyncManager.sync_back()
 */

#include "hermes_core_types.h"
