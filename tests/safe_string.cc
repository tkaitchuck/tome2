#include "tome/safe_string.hpp"
#include <bandit/bandit.h>
#include <string>
using namespace bandit;

go_bandit([]() {

	describe("safe_string", []() {

		it("copy_str copies short strings", [&] {
			char buf[8];
			copy_str(buf, "abc");
			AssertThat(std::string(buf), Equals("abc"));
		});

		it("copy_str truncates and terminates", [&] {
			char buf[4];
			copy_str(buf, "abcdefgh");
			AssertThat(std::string(buf), Equals("abc"));
		});

		it("append_str appends within bounds", [&] {
			char buf[16] = "foo";
			append_str(buf, "bar");
			AssertThat(std::string(buf), Equals("foobar"));
		});

		it("append_str truncates and terminates", [&] {
			char buf[6] = "foo";
			append_str(buf, "barbaz");
			AssertThat(std::string(buf), Equals("fooba"));
		});

		it("TOME_SNPRINTF truncates and terminates", [&] {
			char buf[5];
			std::string const src("abcdefgh");
			TOME_SNPRINTF(buf, "%s", src.c_str());
			AssertThat(std::string(buf), Equals("abcd"));
		});
	});
});
