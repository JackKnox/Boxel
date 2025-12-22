#include "defines.h"
#include "utils/string_utils.h"

#include <stdarg.h> // For variadic functions
#include <stdio.h>  // vsnprintf, sscanf, sprintf
#include <string.h>

#include "platform/platform.h"

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
    char* copy = platform_copy_memory(platform_allocate(length + 1, FALSE), str, length);

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

char* string_format(const char* format, ...) {
    if (!format) return NULL;

    va_list args;
    va_start(args, format);
    char* result = string_format_v(format, args);
    va_end(args);
    return result;
}

char* string_format_v(const char* format, va_list args) {
    if (!format) return NULL;

    va_list list_copy;
    va_copy(list_copy, args);
    u64 length = vsnprintf(NULL, 0, format, list_copy);
    va_end(list_copy);

    char* buffer = platform_allocate(length + 1, FALSE);
    if (!buffer) return NULL;

    vsnprintf(buffer, length + 1, format, args);
    buffer[length] = '\0';
    return buffer;
}