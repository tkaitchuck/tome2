/*
 * libFuzzer harness for the savefile primitive layer.
 *
 * Feeds arbitrary bytes to every container/string reader in
 * savefile_io.hpp. The goal is "no crash, hang or sanitizer report",
 * whatever the input. Build with -DTOME_BUILD_FUZZERS=ON using clang.
 */
#include "savefile_io.hpp"

#include <cstdio>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

using namespace savefile_io;

namespace {

template<typename F>
void with_stream(std::uint8_t const *data, std::size_t size, F f)
{
	FILE *fp = std::tmpfile();
	if (!fp) return;
	if (size) std::fwrite(data, 1, size, fp);
	std::rewind(fp);
	file() = fp;
	f();
	file() = nullptr;
	std::fclose(fp);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const *data, std::size_t size)
{
	auto const L = ls_flag_t::LOAD;

	with_stream(data, size, [&] {
		std::string s;
		do_std_string(s, L);
	});

	with_stream(data, size, [&] {
		std::vector<u16b> v;
		do_vector(L, v, [](u16b *x, ls_flag_t fl) { do_u16b(x, fl); });
	});

	with_stream(data, size, [&] {
		s16b a[8] = {};
		do_array("fuzz", L, a, 8, [](s16b *x, ls_flag_t fl) { do_s16b(x, fl); });
	});

	with_stream(data, size, [&] {
		std::map<u32b, u32b> m{{1, 0}, {2, 0}};
		do_fixed_map(L, m,
			[](u32b *x, ls_flag_t fl) { do_u32b(x, fl); },
			[](u32b &x, ls_flag_t fl) { do_u32b(&x, fl); });
	});

	with_stream(data, size, [&] {
		std::unordered_set<u32b> set;
		do_unordered_set(L, set, [](u32b *x, ls_flag_t fl) { do_u32b(x, fl); });
	});

	with_stream(data, size, [&] {
		boost::optional<u32b> o;
		do_boost_optional(o, L, [](u32b *x, ls_flag_t fl) { do_u32b(x, fl); });
	});

	return 0;
}
