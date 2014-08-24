SRC=$HOME/src
INST=$SRC/install
./configure \
    --prefix=$INST \
    --with-sqlite4=$SRC/sqlite4 \
    CXXFLAGS="-g -Werror -Wunused -O0 -g -O0 -I$INST/include" \
    CFLAGS="-Werror -Wunused -std=gnu99 -g -O0 -I$INST/include" \
    LDFLAGS="-L$INST/lib" \
    CPPFLAGS="-I$INST/include" \
    PKG_CONFIG_PATH=$INST/lib/pkgconfig
