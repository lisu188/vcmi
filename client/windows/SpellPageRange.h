/*
 * SpellPageRange.h, part of VCMI engine
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>

namespace SpellWindowDetail
{
inline std::pair<std::size_t, std::size_t> pageRange(std::size_t count, int page, int spellsPerPage, bool schoolTab)
{
	if(spellsPerPage <= 0)
		return {0, 0};
	const auto pageSize = static_cast<std::size_t>(spellsPerPage);
	const auto firstPageSize = schoolTab ? static_cast<std::size_t>(std::max(0, spellsPerPage - 2)) : pageSize;
	std::size_t offset = 0;
	auto length = firstPageSize;
	if(page > 0)
	{
		const auto previous = schoolTab ? firstPageSize : 0;
		const auto index = static_cast<std::size_t>(schoolTab ? page - 1 : page);
		// Bound by the actual filtered vector before multiplying or creating
		// iterators. This also handles a stale saved/UI page safely.
		if(previous > count || index > (count - previous) / pageSize)
			return {count, 0};
		offset = previous + index * pageSize;
		length = pageSize;
	}
	return {offset, std::min(length, count - offset)};
}
}
