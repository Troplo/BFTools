#ifndef _WIMLIB_ERROR_H
#define _WIMLIB_ERROR_H

#include <stdio.h>

#if __has_include("wimlib.h")
#include "wimlib.h" /* Get error code definitions */
#else
#define WIMLIB_ERR_SUCCESS 0
#define WIMLIB_ERR_INVALID_PARAM 39
#define WIMLIB_ERR_NOMEM 48
#endif

#include "wimlib/compiler.h"
#include "wimlib/types.h"

void _format_attribute(printf, 1, 2) __attribute__((cold))
wimlib_error(const tchar* format, ...);

void _format_attribute(printf, 1, 2) __attribute__((cold))
wimlib_error_with_errno(const tchar* format, ...);

void _format_attribute(printf, 1, 2) __attribute__((cold))
wimlib_warning(const tchar* format, ...);

void _format_attribute(printf, 1, 2) __attribute__((cold))
wimlib_warning_with_errno(const tchar* format, ...);

#define ERROR(format, ...) wimlib_error(T(format), ##__VA_ARGS__)
#define ERROR_WITH_ERRNO(format, ...)                                          \
    wimlib_error_with_errno(T(format), ##__VA_ARGS__)
#define WARNING(format, ...) wimlib_warning(T(format), ##__VA_ARGS__)
#define WARNING_WITH_ERRNO(format, ...)                                        \
    wimlib_warning_with_errno(T(format), ##__VA_ARGS__)

extern bool wimlib_print_errors;
extern FILE* wimlib_error_file;

void print_byte_field(const u8* field, size_t len, FILE* out);

#endif /* _WIMLIB_ERROR_H */
