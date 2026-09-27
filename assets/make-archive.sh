#!/usr/bin/env sh

tar -cf archive.tar $@
zstd archive.tar -19
xxd -i -n buffer archive.tar.zst > archive.tar.zst.h
sed -i 's/unsigned char buffer/static constexpr unsigned char buffer/g' archive.tar.zst.h
sed -i 's/unsigned int buffer_len.*$//g' archive.tar.zst.h
stat --printf="constexpr auto uncompressed_size = %s;\n" archive.tar >> archive.tar.zst.h
rm archive.tar archive.tar.zst
