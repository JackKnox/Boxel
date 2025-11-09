#pragma once

#include "defines.h"

// Holds a handle to a file.
typedef struct file_handle {
    // Opaque handle to internal file handle.
    void* handle;

    // Indicates if this handle is valid.
    b8 is_valid;
} file_handle;

// File open modes. Can be combined.
typedef enum file_modes {
    // Read mode 
    FILE_MODE_READ = 0x1,

    // Write mode 
    FILE_MODE_WRITE = 0x2
} file_modes;

typedef enum file_seek_origin {
    FILE_SEEK_START,
    FILE_SEEK_CURRENT,
    FILE_SEEK_END,
} file_seek_origin;

// Checks if a file with the given path exists.
b8 filesystem_exists(const char* path);

// Attempt to open file located at path.
b8 filesystem_open(const char* path, file_modes mode, b8 binary, file_handle* out_handle);

// Closes the provided handle to a file.
void filesystem_close(file_handle* handle);

// Attempts to read the size of the file to which handle is attached.
b8 filesystem_size(file_handle* handle, u64* out_size);

b8 filesystem_seek(file_handle* handle, i64 offset, file_seek_origin origin);

u64 filesystem_tell(file_handle* handle);

// Reads up to a newline or EOF.
b8 filesystem_read_line(file_handle* handle, u64 max_length, char** line_buf, u64* out_line_length);

// Writes text to the provided file, appending a '\n' afterward.
b8 filesystem_write_line(file_handle* handle, const char* text);

// Reads up to data_size bytes of data into out_bytes_read. Allocates out_data, which must be freed by the caller.
b8 filesystem_read(file_handle* handle, u64 data_size, void* out_data, u64* out_bytes_read);

// Reads all bytes of data into out_bytes.
b8 filesystem_read_all_bytes(file_handle* handle, u8* out_bytes, u64* out_bytes_read);

// Reads all characters of data into out_text.
b8 filesystem_read_all_text(file_handle* handle, char* out_text, u64* out_bytes_read);

// Writes provided data to the file.
b8 filesystem_write(file_handle* handle, u64 data_size, const void* data, u64* out_bytes_written);

//  Opens and reads all content of the file at the provided path.
const void* filesystem_read_entire_binary_file(const char* filepath, u64* out_size);