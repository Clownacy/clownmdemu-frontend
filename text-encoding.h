#ifndef TEXT_ENCODING_H
#define TEXT_ENCODING_H

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Returns UTF-32 codepoint. */
/* Reads a maximum of two bytes. */
cc_u16l ShiftJISToUTF32(const unsigned char* const in_buffer, cc_u8f* const bytes_read);

/* Returns number of bytes written (maximum 4). */
cc_u8f UTF32ToUTF8(unsigned char* const out_buffer, const cc_u32f utf32_codepoint);

#ifdef __cplusplus
}
#endif

#endif /* TEXT_ENCODING_H */
