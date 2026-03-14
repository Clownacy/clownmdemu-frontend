#!/usr/bin/env sh

tar -cf archive.tar $@
xz -kzfF lzma archive.tar
xxd -i -n buffer archive.tar.lzma > archive.tar.lzma.h
sed -i 's/unsigned int buffer_len.*$//g' archive.tar.lzma.h
stat --printf="constexpr auto uncompressed_size = %s;\n" archive.tar >> archive.tar.lzma.h
rm archive.tar archive.tar.lzma
