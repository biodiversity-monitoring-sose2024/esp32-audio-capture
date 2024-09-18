{ rev ? "v2.7"
, sha256 ? "sha256-rO8+coL855xn2cqFT/BINp4uJbgUHMXUQP9DEOXf5hQ="
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