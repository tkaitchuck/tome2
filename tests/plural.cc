#include "tome/plural.hpp"
#include <bandit/bandit.h>
using namespace bandit;

go_bandit([]() {

	describe("pluralize_monster_name", []() {

		it("adds s to ordinary names", [&] {
			AssertThat(pluralize_monster_name("kobold"), Equals("kobolds"));
		});

		it("adds es after s and ch", [&] {
			AssertThat(pluralize_monster_name("kelp"), Equals("kelps"));
			AssertThat(pluralize_monster_name("bus"), Equals("buses"));
			AssertThat(pluralize_monster_name("leech"), Equals("leeches"));
		});

		it("handles the irregular endings", [&] {
			AssertThat(pluralize_monster_name("fly"), Equals("flies"));
			AssertThat(pluralize_monster_name("house"), Equals("hice"));
			AssertThat(pluralize_monster_name("Bandelkelman"), Equals("Bandelkelmen"));
			AssertThat(pluralize_monster_name("vortex"), Equals("vortices"));
			AssertThat(pluralize_monster_name("wolf"), Equals("wolves"));
		});

		it("pluralizes the head of 'X of Y' names", [&] {
			AssertThat(pluralize_monster_name("Knight of the Rose"), Equals("Knights of the Rose"));
			AssertThat(pluralize_monster_name("Priest of Ares"), Equals("Priests of Ares"));
			AssertThat(pluralize_monster_name("Boss of Y"), Equals("Bosses of Y"));
		});

		it("handles the special cases", [&] {
			AssertThat(pluralize_monster_name("Disembodied hand"), Equals("Disembodied hands that strangled people"));
			AssertThat(pluralize_monster_name("creeping gold coins"), Equals("piles of creeping gold coins"));
			AssertThat(pluralize_monster_name("Manes"), Equals("Manes"));
		});

		it("does not overflow on very long names", [&] {
			std::string const name(500, 'x');
			AssertThat(pluralize_monster_name(name), Equals(name + "s"));
		});

		it("accepts the empty string", [&] {
			AssertThat(pluralize_monster_name(""), Equals("s"));
		});
	});
});
