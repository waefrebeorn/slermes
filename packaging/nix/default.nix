# Slermes Nix Derivation
# Use: nix-build packaging/nix/default.nix
# Or:  nix-shell packaging/nix/shell.nix

{ pkgs ? import <nixpkgs> {} }:

pkgs.stdenv.mkDerivation rec {
  pname = "slermes";
  version = "502.0.0";

  src = ../..;

  nativeBuildInputs = with pkgs; [
    pkg-config
    gcc
    make
  ];

  buildInputs = with pkgs; [
    openssl
  ];

  buildPhase = ''
    make -j$NIX_BUILD_CORES
  '';

  installPhase = ''
    mkdir -p $out/bin
    cp slermes $out/bin/
  '';

  meta = with pkgs.lib; {
    description = "C Language Hermes Agent — self-improving AI assistant";
    homepage = "https://github.com/waefrebeorn/slermes";
    license = licenses.mit;
    maintainers = [];
    platforms = platforms.unix;
  };
}
