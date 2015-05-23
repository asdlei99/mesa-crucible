// Copyright 2015 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice (including the next
// paragraph) shall be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#pragma once

#include <stdarg.h>
#include <stdbool.h>

#include <crucible/cru_macros.h>

#ifdef __cplusplus
extern "C" {
#endif

void cru_log_tag(const char *tag, const char *format, ...) cru_printflike(2, 3);
void cru_loge(const char *format, ...) cru_printflike(1, 2);
void cru_logw(const char *format, ...) cru_printflike(1, 2);
void cru_logi(const char *format, ...) cru_printflike(1, 2);
void cru_logd(const char *format, ...) cru_printflike(1, 2);

void cru_log_tag_v(const char *tag, const char *format, va_list va);
void cru_loge_v(const char *format, va_list va);
void cru_logw_v(const char *format, va_list va);
void cru_logi_v(const char *format, va_list va);
void cru_logd_v(const char *format, va_list va);

/// \brief Right-align the tag separator for all log messages.
///
/// Alignment is useful when running tests, because. it's easier to visually to
/// parse the output if all test names to start on the same column.
void cru_log_align_tags(bool enable);

#define cru_finishme(format, ...) \
    __cru_finishme(__FILE__, __LINE__, format, ##__VA_ARGS__)

void __cru_finishme(const char *file, int line, const char *format, ...) cru_printflike(3, 4);

#ifdef __cplusplus
}
#endif
