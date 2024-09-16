let
  pkgs = import <nixpkgs> {};
  esp-idf = pkgs.callPackage ./nix/esp-idf {};
  esp-adf = pkgs.callPackage ./nix/esp-adf {};
in
pkgs.mkShell {
  name = "esp-project";

  buildInputs = with pkgs; [
    esp-idf
    esp-adf
  ];

  shellHook = ''
    export ADF_PATH=${esp-adf}
    export OPENOCD_SCRIPTS='${esp-idf}/openocd-esp32/share/openocd/scripts'
    '';
}