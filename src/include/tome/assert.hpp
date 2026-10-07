#pragma once

#include <cstdio>
#include <cstdlib>

/**
 * ASSERT(cond): invariant check that stays active in every build type,
 * including Release builds with NDEBUG, unlike <cassert>'s assert().
 * Use it for invariants at module boundaries (array indices derived from
 * save files, game IDs, etc.) where silently continuing means memory
 * corruption. Aborts with file/line information on failure.
 */
#define ASSERT(cond)                                                        \
	do {                                                                \
		if (!(cond)) {                                              \
			std::fprintf(stderr, "%s:%d: ASSERT failed: %s\n",  \
				     __FILE__, __LINE__, #cond);            \
			std::abort();                                       \
		}                                                           \
	} while (0)
