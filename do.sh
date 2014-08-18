SRC=$HOME/src
INST=$SRC/install
./configure \
    --prefix=$INST \
    --with-sqlite4=$SRC/sqlite4 \
    CXXFLAGS="-g -Werror -Wunused -O0 -g -O0 -I$SRC/libowfat-0.29 -I$INST/include" \
    CFLAGS="-Werror -Wunused -std=gnu99 -g -O0 -I$SRC/libowfat-0.29 -I$INST/include" \
    LDFLAGS="-L$INST/lib -L$SRC/libowfat-0.29" \
    CPPFLAGS="-I$INST/include -I$SRC/libowfat-0.29" \
    PKG_CONFIG_PATH=$INST/lib/pkgconfig
