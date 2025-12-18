#include "defines.h"
#include "utils/string_utils.h"

#include <stdarg.h> // For variadic functions
#include <stdio.h>  // vsnprintf, sscanf, sprintf

#include <string.h>

u64 string_length(const char* str) {
    return strlen(str);
}

b8 codepoint_is_upper(i32 codepoint) {
    return (codepoint <= 'Z' && codepoint >= 'A') ||
        (codepoint >= 0xC0 && codepoint <= 0xDF);
}

void string_to_lower(char* str) {
    for (u32 i = 0; str[i]; ++i) {
        if (codepoint_is_upper(str[i])) {
            str[i] += ('a' - 'A');
        }
    }
}

b8 char_is_whitespace(char c) {
    // Source of whitespace characters:
    switch (c) {
    case 0x0009: // character tabulation (\t)
    case 0x000A: // line feed (\n)
    case 0x000B: // line tabulation/vertical tab (\v)
    case 0x000C: // form feed (\f)
    case 0x000D: // carriage return (\r)
    case 0x0020: // space (' ')
        return TRUE;
    default:
        return FALSE;
    }
}

char* string_duplicate(const char* str) {
    if (!str) {
        BX_WARN("string_duplicate called with an empty string. 0/null will be returned.");
        return 0;
    }

    u64 length = string_length(str);
    char* copy = platform_copy_memory(ballocate(length + 1, MEMORY_TAG_CORE), str, length);

    copy[length] = 0;
    return copy;
}

i64 str_ncmp(const char* str0, const char* str1, u32 max_len) {
    if (!str0 && !str1) {
        return 0; // Technically equal since both are null.
    }
    else if (!str0 && str1) {
        // Count the first string as 0 and compare against the second, non-empty string.
        return 0 - str1[0];
    }
    else if (str0 && !str1) {
        // Count the second string as 0. In this case, just return the value of the
        // first char of the first string as str[0] - 0 would just be str[0] anyway.
        return str0[0];
    }
    
    return strncmp(str0, str1, max_len);
}

b8 strings_equal(const char* str0, const char* str1) {
    return str_ncmp(str0, str1, UINT32_MAX) == 0;
}

b8 strings_nequal(const char* str0, const char* str1, u32 max_len) {
    return str_ncmp(str0, str1, max_len) == 0;
}

char* string_format(const char* format, u64* out_length, ...) {
    if (!format) {
        return 0;
    }

    va_list arg_ptr;
    va_start(arg_ptr, format);
    char* result = string_format_v(format, out_length, arg_ptr);
    va_end(arg_ptr);
    return result;
}

char* string_format_v(const char* format, u64* out_length, void* va_listp) {
    if (!format) {
        return 0;
    }

    // Create a copy of the va_listp since vsnprintf can invalidate the elements of the list
    // while finding the required buffer length.
    va_list list_copy;
#ifdef _MSC_VER
    list_copy = va_listp;
#elif defined(BX_PLATFORM_APPLE)
    list_copy = va_listp;
#else
    va_copy(list_copy, va_listp);
#endif
    *out_length = vsnprintf(0, 0, format, list_copy);
    va_end(list_copy);
    char* buffer = ballocate(*out_length + 1, MEMORY_TAG_CORE);
    if (!buffer) {
        return 0;
    }
    vsnprintf(buffer, *out_length + 1, format, va_listp);
    buffer[*out_length] = 0;
    return buffer;
}