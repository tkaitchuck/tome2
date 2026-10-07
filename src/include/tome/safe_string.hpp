#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>

/*
 * Bounds-checked replacements for strcpy/strcat/sprintf on fixed-size
 * char arrays. The destination size is deduced from the array type, so
 * these refuse to compile if handed a bare `char *`; that is deliberate.
 * Output is always NUL-terminated and silently truncated.
 */

template<std::size_t N>
void copy_str(char (&dst)[N], char const *src)
{
	static_assert(N > 0, "empty buffer");
	std::size_t const len = std::strlen(src);
	std::size_t const n = len < N - 1 ? len : N - 1;
	std::memcpy(dst, src, n);
	dst[n] = '\0';
}

template<std::size_t N>
void append_str(char (&dst)[N], char const *src)
{
	static_assert(N > 0, "empty buffer");
	std::size_t const used = strnlen(dst, N);
	if (used >= N - 1) {
		dst[N - 1] = '\0';
		return;
	}
	std::size_t const room = N - 1 - used;
	std::size_t const len = std::strlen(src);
	std::size_t const n = len < room ? len : room;
	std::memcpy(dst + used, src, n);
	dst[used + n] = '\0';
}

#define TOME_SNPRINTF(dst, ...) \
	(static_cast<void>(std::snprintf((dst), sizeof(dst), __VA_ARGS__)))
