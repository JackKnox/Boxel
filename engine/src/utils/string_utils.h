#pragma once

#include "defines.h"

// Gets the number of bytes of the given string, minus the null terminator.
u64 string_length(const char* str);

// Indicates if the provided character is considered whitespace.
b8 char_is_whitespace(char c);

// Duplicates the provided string. Note that this allocates new memory, which should be freed by the caller.
char* string_duplicate(const char* str);

// Finds the first occurrence of the substring needle in the string haystack. The terminating null bytes ('\0') are not compared.
const char* string_find_substr(const char *haystack, const char *needle);

// Case-sensitive string comparison.
b8 strings_equal(const char* str0, const char* str1);

// Case-sensitive string comparison, where comparison stops at max_len.
b8 strings_nequal(const char* str0, const char* str1, u32 max_len);

// Performs string formatting against the given format string and parameters. NOTE: This performs a dynamic allocation and should be freed by the caller.
char* string_format(const char* format, ...);

// Performs variadic string formatting against the given format string and va_list. NOTE: This performs a dynamic allocation and should be freed by the caller.
char* string_format_v(const char* format, void* va_list);
