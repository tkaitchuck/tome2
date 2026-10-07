#include "savefile_io.hpp"
#include <bandit/bandit.h>
#include <cstdio>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>
using namespace bandit;
using namespace savefile_io;

namespace {

// Scoped in-memory stream for the primitives under test.
struct stream {
	stream() { file() = std::tmpfile(); }
	~stream() { std::fclose(file()); file() = nullptr; }
	void rewind() { std::rewind(file()); }
	void put_raw(std::string const &bytes) {
		std::fwrite(bytes.data(), 1, bytes.size(), file());
	}
};

std::string u32_le(u32b v) {
	std::string s;
	for (int i = 0; i < 4; i++) s += static_cast<char>((v >> (8 * i)) & 0xFF);
	return s;
}

} // namespace

go_bandit([]() {

	describe("savefile_io round trips", []() {

		it("integers", [&] {
			stream st;
			byte b = 200; u16b w = 51234; s16b sw = -1234;
			u32b d = 4000000000UL; s32b sd = -123456789; int i = 77; bool t = true;
			do_byte(&b, ls_flag_t::SAVE); do_u16b(&w, ls_flag_t::SAVE);
			do_s16b(&sw, ls_flag_t::SAVE); do_u32b(&d, ls_flag_t::SAVE);
			do_s32b(&sd, ls_flag_t::SAVE); do_int(&i, ls_flag_t::SAVE);
			do_std_bool(&t, ls_flag_t::SAVE);
			st.rewind();
			byte b2 = 0; u16b w2 = 0; s16b sw2 = 0; u32b d2 = 0; s32b sd2 = 0; int i2 = 0; bool t2 = false;
			do_byte(&b2, ls_flag_t::LOAD); do_u16b(&w2, ls_flag_t::LOAD);
			do_s16b(&sw2, ls_flag_t::LOAD); do_u32b(&d2, ls_flag_t::LOAD);
			do_s32b(&sd2, ls_flag_t::LOAD); do_int(&i2, ls_flag_t::LOAD);
			do_std_bool(&t2, ls_flag_t::LOAD);
			AssertThat(b2, Equals(b)); AssertThat(w2, Equals(w)); AssertThat(sw2, Equals(sw));
			AssertThat(d2, Equals(d)); AssertThat(sd2, Equals(sd)); AssertThat(i2, Equals(i));
			AssertThat(t2, Equals(t));
		});

		it("strings, including embedded NULs", [&] {
			stream st;
			std::string s("he\0llo", 6);
			do_std_string(s, ls_flag_t::SAVE);
			st.rewind();
			std::string out;
			do_std_string(out, ls_flag_t::LOAD);
			AssertThat(out, Equals(s));
		});

		it("vectors", [&] {
			stream st;
			std::vector<u16b> v{1, 2, 3, 65535};
			auto f = [](u16b *x, ls_flag_t fl) { do_u16b(x, fl); };
			do_vector(ls_flag_t::SAVE, v, f);
			st.rewind();
			std::vector<u16b> out;
			do_vector(ls_flag_t::LOAD, out, f);
			AssertThat(out == v, Equals(true));
		});

		it("arrays", [&] {
			stream st;
			s16b a[4] = {-1, 2, -3, 4};
			auto f = [](s16b *x, ls_flag_t fl) { do_s16b(x, fl); };
			do_array("things", ls_flag_t::SAVE, a, 4, f);
			st.rewind();
			s16b b[4] = {0, 0, 0, 0};
			do_array("things", ls_flag_t::LOAD, b, 4, f);
			for (int i = 0; i < 4; i++) AssertThat(b[i], Equals(a[i]));
		});

		it("maps, sets and optionals", [&] {
			stream st;
			std::map<u32b, u32b> m{{1, 10}, {2, 20}};
			std::unordered_set<u32b> set{5, 6, 7};
			boost::optional<u32b> opt(42);
			auto fu = [](u32b *x, ls_flag_t fl) { do_u32b(x, fl); };
			auto fr = [](u32b &x, ls_flag_t fl) { do_u32b(&x, fl); };
			do_fixed_map(ls_flag_t::SAVE, m, fu, fr);
			do_unordered_set(ls_flag_t::SAVE, set, fu);
			do_boost_optional(opt, ls_flag_t::SAVE, fu);
			st.rewind();
			std::map<u32b, u32b> m2{{1, 0}, {2, 0}};
			std::unordered_set<u32b> set2;
			boost::optional<u32b> opt2;
			do_fixed_map(ls_flag_t::LOAD, m2, fu, fr);
			do_unordered_set(ls_flag_t::LOAD, set2, fu);
			do_boost_optional(opt2, ls_flag_t::LOAD, fu);
			AssertThat(m2 == m, Equals(true));
			AssertThat(set2 == set, Equals(true));
			AssertThat(static_cast<bool>(opt2), Equals(true));
			AssertThat(*opt2, Equals(42UL));
		});
	});

	describe("savefile_io on corrupt input", []() {

		it("does not overrun an array when the file claims too many entries", [&] {
			stream st;
			st.put_raw(u32_le(1000));
			for (int i = 0; i < 1000; i++) st.put_raw(std::string("\x07\x00", 2));
			st.rewind();
			s16b a[3] = {0, 0, 0};
			// Guard words either side catch out-of-bounds writes
			// even without ASAN.
			struct { s16b lo; s16b a[3]; s16b hi; } g{0x5A5A, {0, 0, 0}, 0x5A5A};
			auto f = [](s16b *x, ls_flag_t fl) { do_s16b(x, fl); };
			do_array("things", ls_flag_t::LOAD, g.a, 3, f);
			AssertThat(g.lo, Equals(0x5A5A));
			AssertThat(g.hi, Equals(0x5A5A));
			AssertThat(g.a[2], Equals(7));
			(void)a;
		});

		it("survives a huge string length on a tiny file", [&] {
			stream st;
			st.put_raw(u32_le(0xFFFFFFFFUL));
			st.put_raw("abc");
			st.rewind();
			std::string s;
			do_std_string(s, ls_flag_t::LOAD);
			AssertThat(s, Equals("abc"));
		});

		it("survives a huge vector length on a tiny file", [&] {
			stream st;
			st.put_raw(u32_le(0xFFFFFFFFUL));
			st.put_raw(std::string("\x01\x00\x02\x00", 4));
			st.rewind();
			std::vector<u16b> v;
			do_vector(ls_flag_t::LOAD, v, [](u16b *x, ls_flag_t fl) { do_u16b(x, fl); });
			AssertThat(v.size() <= 3, Equals(true));
			AssertThat(v[0], Equals(1));
			AssertThat(v[1], Equals(2));
		});

		it("terminates on a huge set/map/optional count", [&] {
			stream st;
			st.put_raw(u32_le(0xFFFFFFFFUL));
			st.rewind();
			std::unordered_set<u32b> set;
			do_unordered_set(ls_flag_t::LOAD, set, [](u32b *x, ls_flag_t fl) { do_u32b(x, fl); });
			AssertThat(set.size() <= 1, Equals(true));
		});
	});
});
