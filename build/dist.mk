# ── Distribution Targets ──────────────────────────────────────────
# AppImage, Docker, NSIS (Windows), Nix builds
# Included by top-level Makefile

.PHONY: dist-appimage dist-docker dist-nsis dist-nix dist-all

dist-appimage:
	@echo "Building AppImage..."
	bash packaging/appimage/build-appimage.sh

dist-docker:
	@echo "Building Docker image..."
	docker build -t slermes:latest -f packaging/docker/Dockerfile .

dist-nsis:
	@echo "Building Windows installer (requires makensis)..."
	makensis packaging/nsis/slermes.nsi

dist-nix:
	@echo "Building Nix derivation..."
	nix-build packaging/nix/default.nix

dist-all: dist-docker
	@echo "=== Distribution packages built ==="
	@echo "  Docker:  slermes:latest"
	@echo "  AppImage: run 'make dist-appimage'"
	@echo "  NSIS:     run 'make dist-nsis' (requires makensis)"
	@echo "  Nix:      run 'make dist-nix'"
