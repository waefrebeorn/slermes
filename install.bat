@echo off
REM ── Slermes Windows Installer ──
REM Requires: WSL, git, make, gcc

echo === Slermes Windows Installer ===
echo.
echo This installer requires WSL (Windows Subsystem for Linux).
echo If you don't have WSL, install it first:
echo   wsl --install -d Ubuntu
echo.
echo Checking for WSL...

wsl.exe --version >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: WSL not found. Please install WSL first.
    echo   wsl --install -d Ubuntu
    pause
    exit /b 1
)

echo WSL found. Checking for build tools...

wsl.exe bash -c "which git make gcc 2>/dev/null" >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Build tools not found. Installing...
    wsl.exe bash -c "sudo apt-get update && sudo apt-get install -y git make gcc libssl-dev pkg-config"
)

echo.
echo Cloning Slermes...
wsl.exe bash -c "git clone https://github.com/waefrebeorn/slermes.git ~/slermes"

echo Building...
wsl.exe bash -c "cd ~/slermes && make -j$(nproc)"

echo Installing...
wsl.exe bash -c "mkdir -p ~/.local/bin && cp ~/slermes/slermes ~/.local/bin/ && ln -sf ~/.local/bin/slermes ~/.local/bin/hermes"

echo.
echo === Installation complete ===
echo.
echo Slermes binary installed at ~/.local/bin/slermes
echo Add to WSL PATH if needed:
echo   export PATH="$HOME/.local/bin:$PATH"
echo.
echo Run: wsl slermes setup
echo.
pause
