#include "defines.h"
#include "filesystem.h"

#include <stdio.h>
#include <sys/stat.h>

b8 filesystem_exists(const char* path) {
#ifdef _MSC_VER
    struct _stat buffer;
    return _stat(path, &buffer) == 0;
#else
    struct stat buffer;
    return stat(path, &buffer) == 0;
#endif
}

b8 filesystem_open(const char* path, file_modes mode, b8 binary, file_handle* out_handle) {
    BX_ASSERT(path != NULL && out_handle != NULL && "Invalid arguments passed to filesystem_open");
    out_handle->is_valid = FALSE;
    out_handle->handle = 0;

    const char* mode_str;
    if ((mode & FILE_MODE_READ) != 0 && (mode & FILE_MODE_WRITE) != 0) {
        mode_str = binary ? "w+b" : "w+";
    }
    else if ((mode & FILE_MODE_READ) != 0 && (mode & FILE_MODE_WRITE) == 0) {
        mode_str = binary ? "rb" : "r";
    }
    else if ((mode & FILE_MODE_READ) == 0 && (mode & FILE_MODE_WRITE) != 0) {
        mode_str = binary ? "wb" : "w";
    }
    else {
        BX_ERROR("Invalid mode passed while trying to open file: '%s'", path);
        return FALSE;
    }

    // Attempt to open the file.
    FILE* file = fopen(path, mode_str);
    if (!file) {
        return FALSE;
    }

    out_handle->handle = file;
    out_handle->is_valid = TRUE;
    return TRUE;
}

void filesystem_close(file_handle* handle) {
    BX_ASSERT(handle != NULL && handle->handle != NULL && handle->is_valid && "Invalid arguments passed to filesystem_close");

    fclose((FILE*)handle->handle);
    handle->handle = 0;
    handle->is_valid = FALSE;
}

b8 filesystem_size(file_handle* handle, u64* out_size) {
    BX_ASSERT(handle != NULL && handle->handle != NULL && handle->is_valid && out_size != NULL && "Invalid arguments passed to filesystem_size");

    if (!filesystem_seek(handle, 0, FILE_SEEK_END)) return FALSE;

    *out_size = filesystem_tell(handle);
    return filesystem_seek(handle, 0, FILE_SEEK_START);
}

b8 filesystem_seek(file_handle* handle, i64 offset, file_seek_origin origin) {
    BX_ASSERT(handle != NULL && handle->handle != NULL && handle->is_valid && "Invalid arguments passed to filesystem_seek");
    return fseek((FILE*)handle->handle, offset, origin) == 0;
}

u64 filesystem_tell(file_handle* handle) {
    BX_ASSERT(handle != NULL && handle->handle != NULL && handle->is_valid && "Invalid arguments passed to filesystem_tell");
    return ftell((FILE*)handle->handle) == 0;
}

b8 filesystem_read_line(file_handle* handle, u64 max_length, char** line_buf, u64* out_line_length) {
    BX_ASSERT(handle != NULL && handle->handle != NULL && handle->is_valid && line_buf != NULL && out_line_length != NULL && max_length > 0 && "Invalid arguments passed to filesystem_read_line");

    char* buf = *line_buf;
    if (fgets(buf, max_length, (FILE*)handle->handle) != 0) {
        *out_line_length = strlen(*line_buf);
        return TRUE;
    }

    return FALSE;
}

b8 filesystem_write_line(file_handle* handle, const char* text) {
    BX_ASSERT(handle != NULL && handle->handle != NULL && handle->is_valid && text != NULL && "Invalid arguments passed to filesystem_write_line");

    i32 result = fputs(text, (FILE*)handle->handle);
    if (result != EOF) {
        result = fputc('\n', (FILE*)handle->handle);
    }

    // Make sure to flush the stream so it is written to the file immediately.
    // This prevents data loss in the event of a crash.
    fflush((FILE*)handle->handle);
    return result != EOF;
}

b8 filesystem_read(file_handle* handle, u64 data_size, void* out_data, u64* out_bytes_read) {
    BX_ASSERT(handle != NULL && handle->handle != NULL && handle->is_valid && out_data != NULL && "Invalid arguments passed to filesystem_read");

    u64 temp_bytes_read = 0;
    if (!out_bytes_read) out_bytes_read = &temp_bytes_read;

    *out_bytes_read = fread(out_data, 1, data_size, (FILE*)handle->handle);
    if (*out_bytes_read != data_size) {
        return FALSE;
    }

    return TRUE;
}

b8 filesystem_read_all_bytes(file_handle* handle, u8* out_bytes, u64* out_bytes_read) {
    BX_ASSERT(handle != NULL && handle->handle != NULL && handle->is_valid && out_bytes != NULL && "Invalid arguments passed to filesystem_read_all_bytes");

    u64 size = 0;
    if (!filesystem_size(handle, &size)) {
        return FALSE;
    }

    return filesystem_read(handle, size, out_bytes, out_bytes_read);
}

b8 filesystem_read_all_text(file_handle* handle, char* out_text, u64* out_bytes_read) {
    BX_ASSERT(handle != NULL && handle->handle != NULL && handle->is_valid && out_text != NULL && "Invalid arguments passed to filesystem_read_all_text");

    u64 size = 0;
    if (!filesystem_size(handle, &size)) {
        return FALSE;
    }

    return filesystem_read(handle, size, out_text, out_bytes_read);
}

b8 filesystem_write(file_handle* handle, u64 data_size, const void* data, u64* out_bytes_written) {
    BX_ASSERT(handle != NULL && handle->handle != NULL && handle->is_valid && data != NULL && out_bytes_written != NULL && "Invalid arguments passed to filesystem_write");

    *out_bytes_written = fwrite(data, 1, data_size, (FILE*)handle->handle);
    if (*out_bytes_written != data_size) {
        return FALSE;
    }

    fflush((FILE*)handle->handle);
    return TRUE;
}

const void* filesystem_read_entire_binary_file(const char* filepath, u64* out_size) {
    BX_ASSERT(filepath != NULL && out_size != NULL && "Invalid arguments passed to filesystem_read_entire_binary_file");

    file_handle f;
    if (!filesystem_open(filepath, FILE_MODE_READ, TRUE, &f)) {
        return 0;
    }

    // File size
    if (!filesystem_size(&f, out_size)) {
        return 0;
    }
    char* buf = ballocate(*out_size, MEMORY_TAG_CORE);
    fread(buf, 1, *out_size, (FILE*)f.handle);

    return buf;
}