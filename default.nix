{ pkgs ? import <nixpkgs> {} }:
let
  gitignoreSource = (import ./gitignoreSource { lib = pkgs.lib; }).gitignoreSource;
in
pkgs.stdenv.mkDerivation {
  name = "reflex-cpp";
  nativeBuildInputs = [ pkgs.cmake pkgs.gcc ];
  buildInputs = [ pkgs.boost ];
  src = gitignoreSource ./.;
}
