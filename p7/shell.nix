with import <nixpkgs> {};

mkShell {
  nativeBuildInputs = [ pkg-config ];
  buildInputs = [ openssl ];
  shellHook = ''
    export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$HOME/UP/2024/COS332/Pracs/p7" && alias run='g++ main.cxx -o main -lssl -lcrypto -DPASSWORD=\"yfcelarqfqjwytli\" && ./main'
  '';
}
