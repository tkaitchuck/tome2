#pragma once

#include <string>

/*
 * Pluralize a monster name ("Grip, Farmer Maggot's Dog" style names are
 * the caller's problem). Pure function; replaces the old in-place
 * char-buffer implementation in cmd4.cc, which overflowed on long names.
 */
inline std::string pluralize_monster_name(std::string name)
{
	auto ends_with = [&](char const *suffix) {
		std::string const s(suffix);
		return name.size() >= s.size() &&
			name.compare(name.size() - s.size(), s.size(), s) == 0;
	};
	auto replace_tail = [&](std::size_t n, char const *tail) {
		name.erase(name.size() - n);
		name += tail;
	};

	/* Hack -- Precedent must be pluralised for this one */
	if (name.find("Disembodied hand") != std::string::npos) {
		return "Disembodied hands that strangled people";
	}

	/* "someone of something" */
	auto const of = name.find(" of ");
	if (of != std::string::npos) {
		std::string head = name.substr(0, of);
		std::string const tail = name.substr(of);
		bool const ends_s = !head.empty() && head.back() == 's';
		head += ends_s ? "es" : "s";
		return head + tail;
	}

	/* Creeping coins */
	if (name.find("coins") != std::string::npos) {
		return "piles of " + name;
	}

	/* Manes stay manes */
	if (name.find("Manes") != std::string::npos) {
		return name;
	}

	/* Broken plurals are, well, broken */
	if (ends_with("y")) {
		replace_tail(1, "ies");
	} else if (ends_with("ouse")) {
		replace_tail(4, "ice");
	} else if (ends_with("kelman")) {
		replace_tail(6, "kelmen");
	} else if (ends_with("ex")) {
		replace_tail(2, "ices");
	} else if (ends_with("olf")) {
		replace_tail(3, "olves");
	} else if (ends_with("ch") || ends_with("s")) {
		name += "es";
	} else {
		name += "s";
	}
	return name;
}
