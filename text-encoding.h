#ifndef TEXT_ENCODING_H
#define TEXT_ENCODING_H

#include <optional>
#include <string>

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

/* Returns UTF-32 codepoint. */
/* Reads a maximum of two bytes. */
cc_u16l ShiftJISToUTF32(const unsigned char* const in_buffer, cc_u8f* const bytes_read);

/* Returns number of bytes written (maximum 4). */
std::optional<std::string> UTF32ToUTF8(const cc_u32f utf32_codepoint);

#endif /* TEXT_ENCODING_H */
