#pragma once

/*
 * Low-level savefile serialization primitives, shared by loadsave.cc,
 * the unit tests and the fuzz harness.
 *
 * All routines read/write through the FILE* returned by file(). Every
 * loop whose trip count comes from the file stops at end-of-file, and
 * no allocation is sized from an unvalidated count, so a truncated or
 * corrupt save cannot hang the loader, exhaust memory, or write out of
 * bounds.
 */

#include "h-basic.hpp"

#include <algorithm>
#include <boost/optional.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

namespace savefile_io {

/**
 * Load/save flag
 */
enum class ls_flag_t {
	LOAD = 3,
	SAVE = 7
};

/**
 * The stream all primitives operate on.
 */
inline FILE *&file()
{
	static FILE *f = nullptr;
	return f;
}

/**
 * Optional sink for non-fatal load warnings (e.g. the in-game note()).
 */
inline void (*&warn_hook())(char const *)
{
	static void (*hook)(char const *) = nullptr;
	return hook;
}

inline void warn(std::string const &msg)
{
	if (warn_hook()) {
		warn_hook()(msg.c_str());
	}
}

/**
 * True once the stream is unusable or exhausted.
 */
inline bool at_eof()
{
	FILE *f = file();
	return !f || std::feof(f) || std::ferror(f);
}

/*
 * Basic byte-level reading from savefile.
 */
inline byte sf_get()
{
	return std::getc(file()) & 0xFF;
}

inline void sf_put(byte v)
{
	std::putc(static_cast<int>(v), file());
}

/*
 * Size-aware read/write routines for the savefile, do all their
 * work through sf_get and sf_put.
 */
inline void do_byte(byte *v, ls_flag_t flag)
{
	switch (flag) {
	case ls_flag_t::LOAD:
		*v = sf_get();
		return;
	case ls_flag_t::SAVE:
		sf_put(*v);
		return;
	}
}

inline void do_char(char *c, ls_flag_t flag)
{
	do_byte(reinterpret_cast<byte *>(c), flag);
}

inline void do_std_bool(bool *x, ls_flag_t flag)
{
	switch (flag) {
	case ls_flag_t::LOAD:
		*x = (sf_get() != 0);
		return;
	case ls_flag_t::SAVE:
		sf_put((*x) ? 1 : 0);
		return;
	}
}

inline void do_u16b(u16b *v, ls_flag_t flag)
{
	switch (flag) {
	case ls_flag_t::LOAD:
		(*v) = sf_get();
		(*v) |= (static_cast<u16b>(sf_get()) << 8);
		return;
	case ls_flag_t::SAVE: {
		u16b val = *v;
		sf_put(static_cast<byte>(val & 0xFF));
		sf_put(static_cast<byte>((val >> 8) & 0xFF));
		return;
	}
	}
}

inline void do_s16b(s16b *ip, ls_flag_t flag)
{
	do_u16b(reinterpret_cast<u16b *>(ip), flag);
}

inline void do_u32b(u32b *ip, ls_flag_t flag)
{
	switch (flag) {
	case ls_flag_t::LOAD:
		(*ip) = sf_get();
		(*ip) |= (static_cast<u32b>(sf_get()) << 8);
		(*ip) |= (static_cast<u32b>(sf_get()) << 16);
		(*ip) |= (static_cast<u32b>(sf_get()) << 24);
		return;
	case ls_flag_t::SAVE: {
		u32b val = *ip;
		sf_put(static_cast<byte>(val & 0xFF));
		sf_put(static_cast<byte>((val >> 8) & 0xFF));
		sf_put(static_cast<byte>((val >> 16) & 0xFF));
		sf_put(static_cast<byte>((val >> 24) & 0xFF));
		return;
	}
	}
}

inline void do_s32b(s32b *ip, ls_flag_t flag)
{
	do_u32b(reinterpret_cast<u32b *>(ip), flag);
}

inline void do_int(int *sz, ls_flag_t flag)
{
	u32b x = 0;

	if (flag == ls_flag_t::SAVE) {
		x = *sz;
	}

	do_u32b(&x, flag);

	if (flag == ls_flag_t::LOAD) {
		*sz = x;
	}
}

inline void save_std_string(std::string const *s)
{
	// Length prefix.
	u32b saved_size = s->size();
	do_u32b(&saved_size, ls_flag_t::SAVE);
	// Save each character
	for (auto c : *s) {
		sf_put(c);
	}
}

inline std::string load_std_string()
{
	// Length prefix.
	u32b saved_size = 0;
	do_u32b(&saved_size, ls_flag_t::LOAD);
	std::size_t const n = saved_size;
	// The length comes from the file, so don't trust it for a
	// reservation; the string grows as bytes actually arrive.
	std::string s;
	s.reserve(std::min<std::size_t>(n, 4096));
	for (std::size_t i = 0; i < n; i++) {
		byte const c = sf_get();
		if (at_eof()) {
			break;
		}
		s += static_cast<char>(c);
	}
	return s;
}

inline void do_std_string(std::string &s, ls_flag_t flag)
{
	switch (flag) {
	case ls_flag_t::LOAD:
		s = load_std_string();
		break;
	case ls_flag_t::SAVE:
		save_std_string(&s);
		break;
	}
}

template<typename T, typename F>
void do_vector(ls_flag_t flag, std::vector<T> &v, F f)
{
	u32b n = v.size();

	do_u32b(&n, flag);

	if (flag == ls_flag_t::LOAD) {
		v.clear(); // Make sure it's empty
		// Grow as elements actually arrive; n is untrusted.
		for (u32b i = 0; i < n && !at_eof(); i++) {
			v.emplace_back();
			f(&v.back(), flag);
		}
		return;
	}

	for (std::size_t i = 0; i < n; i++) {
		f(&v[i], flag);
	}
}

template<typename A, typename F>
void do_array(std::string const &what, ls_flag_t flag, A &array, std::size_t size, F f)
{
	// Save/load size.
	u32b n = size;
	do_u32b(&n, flag);

	if (flag == ls_flag_t::SAVE) {
		for (std::size_t i = 0; i < n; i++) {
			f(&array[i], flag);
		}
		return;
	}

	if (n > size) {
		warn("Too many " + what + ": " + std::to_string(n) + " > " +
		     std::to_string(size) + "! Extra entries ignored.");
	}

	// Load the contents of the array. Entries past the end of the
	// array are read (to stay in sync with the file) and discarded.
	using elem_t = std::remove_reference_t<decltype(array[0])>;
	for (std::size_t i = 0; i < n && !at_eof(); i++) {
		if (i < size) {
			f(&array[i], flag);
		} else {
			elem_t scratch{};
			f(&scratch, flag);
		}
	}
}

template<typename M, typename FK, typename FV>
void do_fixed_map(ls_flag_t flag, M &map, FK fk, FV fv)
{
	// Since our file format is currently quite inflexible, we'll
	// have to prefix with the size of the map and store everything
	// as key-value pairs.
	u32b n = map.size();
	do_u32b(&n, flag);

	if (flag == ls_flag_t::LOAD) {
		// Read each of the n entries. We ignore data for keys
		// which no longer exist. This is pretty common if e.g.
		// game data gets removed.
		for (std::size_t i = 0; i < n && !at_eof(); i++) {
			// Read key
			typename M::key_type key;
			fk(&key, flag);
			// If the key is present, we'll update the value
			// by reading. Otherwise just read into a dummy
			// value.
			if (map.count(key)) {
				fv(map.at(key), flag);
			} else {
				typename M::mapped_type v;
				fv(v, flag);
			}
		}
	}

	if (flag == ls_flag_t::SAVE) {
		// Write each of the n entries.
		for (auto &entry : map) {
			auto key = entry.first;
			auto value = entry.second;
			fk(&key, flag);
			fv(value, flag);
		}
	}
}

template<typename S, typename F>
void do_unordered_set(ls_flag_t flag, S &set, F f)
{
	// Since our file format is currently quite inflexible, we'll
	// have to prefix with the size of the set.
	u32b n = set.size();
	do_u32b(&n, flag);

	if (flag == ls_flag_t::LOAD) {
		// Read each of the n entries.
		for (std::size_t i = 0; i < n && !at_eof(); i++) {
			typename S::key_type key;
			f(&key, flag);
			set.insert(key);
		}
	}

	if (flag == ls_flag_t::SAVE) {
		// We must copy out the entries because the 'f' function
		// takes a non-const argument (for loading) and iterating
		// through the set only gives us 'const' access to the keys.
		std::vector<typename S::key_type> keys;
		std::copy(std::cbegin(set), std::cend(set), std::back_inserter(keys));

		for (auto &key : keys) {
			f(&key, flag);
		}
	}
}

inline void do_bytes(ls_flag_t flag, std::uint8_t *buf, std::size_t n)
{
	for (std::size_t i = 0; i < n; i++) {
		do_byte(&buf[i], flag);
	}
}

template<typename T, typename F>
void do_boost_optional(boost::optional<T> &maybe_v, ls_flag_t flag, F f)
{
	if (flag == ls_flag_t::SAVE) {
		// Size
		u32b n = maybe_v ? 1 : 0;
		do_u32b(&n, flag);

		// Value
		if (maybe_v) {
			auto v = *maybe_v;
			f(&v, flag);
		}
	}

	if (flag == ls_flag_t::LOAD) {
		// Size
		u32b n = 0;
		do_u32b(&n, flag);

		// Value
		while (n-- > 0 && !at_eof()) {
			maybe_v.emplace(); // Default-construct in place
			f(&maybe_v.get(), flag);
		}
	}
}

} // namespace savefile_io
