#!/usr/bin/env sh

tar -cf fonts.tar Inconsolata-Regular.ttf NotoSansJP-Regular.ttf
xz -kzfF lzma fonts.tar
xxd -i -n buffer fonts.tar.lzma > fonts.tar.lzma.h
sed -i 's/unsigned int buffer_len.*$//g' fonts.tar.lzma.h
stat --printf="constexpr auto uncompressed_size = %s;\n" fonts.tar >> fonts.tar.lzma.h
rm fonts.tar fonts.tar.lzma
