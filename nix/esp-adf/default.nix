{ rev ? "d0381eb"
, sha256 ? "sha256-OMrU94xHqoHsajscUFmVuZunEk1zMG6XprOf8224/j8="
, stdenv
, lib
, fetchFromGitHub
, makeWrapper
, callPackage

, python3

  # Tools for using ESP-IDF.
, git
, wget
, gnumake
, flex
, bison
, gperf
, pkg-config
, cmake
, ninja
, ncurses5
, dfu-util
} : stdenv.mkDerivation rec {
  pname = "esp-adf";
  version = rev;

  src = fetchFromGitHub {
    owner = "espressif";
    repo = "esp-adf";
    inherit rev sha256;
    fetchSubmodules = true;
  };

  nativeBuildInputs = [ makeWrapper ];

  installPhase = ''
    mkdir -p $out
    cp -rv . $out/
  '';
}