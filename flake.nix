{
  description = "Slermes — Full C translation of Hermes Agent";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "slermes";
          version = "0.15.1";
          src = ./.;

          nativeBuildInputs = with pkgs; [ pkg-config gcc ];
          buildInputs = with pkgs; [ openssl zlib ];
          enableParallelBuilding = true;

          buildFlags = [ "slermes" ];

          installPhase = ''
            mkdir -p $out/bin
            cp slermes $out/bin/
            mkdir -p $out/share/slermes
            cp -r assets $out/share/slermes/ 2>/dev/null || true
          '';

          meta = with pkgs.lib; {
            description = "C translation of Hermes Agent by Nous Research";
            homepage = "https://github.com/waefrebeorn/slermes";
            license = licenses.mit;
            platforms = platforms.linux;
            maintainers = [];
          };
        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [ openssl zlib gcc pkg-config ];
          shellHook = ''
            echo "Slermes dev shell. Run 'make -j\$(nproc)' to build."
          '';
        };
      });
}
