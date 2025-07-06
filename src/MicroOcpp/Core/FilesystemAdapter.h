// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_FILESYSTEMADAPTER_H
#define MO_FILESYSTEMADAPTER_H

#include <stddef.h>
#include <MicroOcpp/Platform.h>

#define MO_CUSTOM_FS       0
#define MO_ARDUINO_LITTLEFS 1
#define MO_ARDUINO_SPIFFS   2
#define MO_ESPIDF_SPIFFS    3
#define MO_POSIX_FILEAPI    4

// choose FileAPI if not given by build flag; assume usage with Arduino if no build flags are present
#ifndef MO_USE_FILEAPI
#if MO_PLATFORM == MO_PLATFORM_ARDUINO
#if defined(ESP32)
#define MO_USE_FILEAPI MO_ARDUINO_LITTLEFS
#else
#define MO_USE_FILEAPI MO_ARDUINO_SPIFFS
#endif
#elif MO_PLATFORM == MO_PLATFORM_ESPIDF
#define MO_USE_FILEAPI MO_ESPIDF_SPIFFS
#elif MO_PLATFORM == MO_PLATFORM_UNIX
#define MO_USE_FILEAPI MO_POSIX_FILEAPI
#else
#define MO_USE_FILEAPI MO_CUSTOM_FS
#endif //switch-case MO_PLATFORM
#endif //ndef MO_USE_FILEAPI

#ifndef MO_FILENAME_PREFIX
#if MO_USE_FILEAPI == MO_ESPIDF_SPIFFS
#define MO_FILENAME_PREFIX "/mo_store/"
#elif MO_PLATFORM == MO_PLATFORM_UNIX
#define MO_FILENAME_PREFIX "./mo_store/"
#else
#define MO_FILENAME_PREFIX "/"
#endif
#endif

// set default max path size parameters
#ifndef MO_MAX_PATH_SIZE
#if MO_USE_FILEAPI == MO_POSIX_FILEAPI
#define MO_MAX_PATH_SIZE 128
#else
#define MO_MAX_PATH_SIZE 30
#endif
#endif

#ifndef MO_ENABLE_FILE_INDEX
#define MO_ENABLE_FILE_INDEX 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct MO_File;
typedef struct MO_File MO_File;

typedef struct {
    /* Path of the MO root folder, plus trailing '/'. MO will create the path for accessing the filesystem by appending
     * a filename to the end of path_prefix. Can be empty if the filesystem implementation accepts the MO filenames as
     * path directly. MO doesn't use sub-directories. */
    const char *path_prefix;

    /* Checks if a file at `path` exists and if yes, writes the size of the file content into `size` and returns 0. Returns
     * a non-zero code if the file doesn't exist, is a directory or the operation was unsuccessful.
     * `path`: path to the file, determined by path_prefix and the MO file name
     * `size`: output param for the file content size */
    int (*stat)(const char *path, size_t *size);

    /* Removes the file at `path`. Returns true if after this operation, no file at this path exists anymore, i.e. either
     * the file was removed or there was no file. Returns false if the file couldn't be removed.
     * `path`: path to the file, determined by path_prefix and the MO file name */
    bool (*remove)(const char *path);

    /* Enumerates all files in the folder at `path` and executes the callback function `fn()` for each file. Passes two
     * arguments to `fn()`, the name of the enumerated file (!= path) and the `mo_user_data` pointer which is provided
     * to `ftw()`. `ftw()` returns 0 if all `fn()` executions were successful, or the non-zero error code of the failing
     * `fn()` execution.
     * `path_prefix`: path to the directory to enumerate. `path` may contain a superfluous "/" at the end
     * `fn`: callback function to execute for each file. Returns 0 on success and a non-zero value to abort `ftw()`
     * `mo_user_data`: pointer to pass through to `fn()` */
    int (*ftw)(const char *path_prefix, int(*fn)(const char *fname, void *mo_user_data), void *mo_user_data);

    /* Opens the file at `path`. The `mode` is either "w" (write) or "r" (read). Returns an implementation-specific file
     * handle on success, or NULL on failure.
     * `path`: path to the file, determined by path_prefix and the MO file name
     * `mode`: "w" (write) or "r" (read) mode */
    MO_File* (*open)(const char *path, const char *mode);

    /* Closes a file which has been opened with `open`. Returns `true` on success. For files in the write mode the success
     * code also indicates if the file writes were successful. Deallocates the resources regargless of the error code.
     * `file`: file handle */
    bool (*close)(MO_File *file);

    /* Reads `file` contents into `buf` with `len` bytes bufsize. Returns the number of read bytes, or 0 on error or if end
     * of file has been reached. If the return value is equal to `len`, MO will exeucte `read` again.
     * `file`: file handle
     * `buf`: read buffer
     * `len`: bufsize in bytes */
    size_t (*read)(MO_File *file, char *buf, size_t len);

    int (*getc)(MO_File *file);

    /* Writes `buf` (with `len` bytes) into `file`. Returns the number of bytes actually written. If the return value is
     * less than `len`, that indicates an error and MO will stop writing to this file.
     * `file`: file handle
     * `buf`: write buffer
     * `len`: bufsize in bytes */
    size_t (*write)(MO_File *file, const char *buf, size_t len);

    /* Sets the file read- or write-position to `fpos`. For example, if `file` has been opened in read mode and `fpos` is
     * 5, then the next `read` operation will continue from the 5th byte of the file. Returns 0 on success and a non-zero
     * error code on failue.
     * `file`: file handle
     * `fpos`: file position */
    int (*seek)(MO_File *file, size_t fpos);

    void *mo_data; //reserved for MO internal usage
} MO_FilesystemAdapter;

/*
 * Platform specific implementation. Currently supported:
 *     - Arduino LittleFs
 *     - Arduino SPIFFS
 *     - ESP-IDF SPIFFS
 *     - POSIX-like API (tested on Ubuntu 20.04)
 *
 * You can add support for other file systems by passing a custom adapter to mocpp_initialize(...)
 *
 * Returns null if platform is not supported or Filesystem is disabled
 */

#if MO_USE_FILEAPI != MO_CUSTOM_FS

typedef enum {
    MO_FS_OPT_DISABLE,
    MO_FS_OPT_USE,
    MO_FS_OPT_USE_MOUNT,
    MO_FS_OPT_USE_MOUNT_FORMAT_ON_FAIL
} MO_FilesystemOpt;

typedef struct {
    MO_FilesystemOpt opt; //FS access level and mounting strategy

    /* Path of the MO root folder, plus trailing '/'. MO will create the path for accessing the filesystem by appending
     * a filename to the end of path_prefix. Can be empty if the filesystem implementation accepts the MO filenames as
     * path directly. MO doesn't use sub-directories. */
    const char *path_prefix;
} MO_FilesystemConfig;
MO_FilesystemAdapter *mo_makeDefaultFilesystemAdapter(MO_FilesystemConfig config);
void mo_freeDefaultFilesystemAdapter(MO_FilesystemAdapter *filesystem);

#endif //MO_USE_FILEAPI != MO_CUSTOM_FS

#ifdef __cplusplus
}
#endif

#endif
