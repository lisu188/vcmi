/*
 * PathfinderCapacity.h, part of VCMI engine
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace NK2AI
{
inline int checkedPathfinderCapacity(std::int64_t bucketCount, std::int64_t bucketSize)
{
	if(bucketCount <= 0 || bucketSize <= 0 || bucketCount > std::numeric_limits<int>::max() / bucketSize)
		throw std::invalid_argument("Pathfinder bucket dimensions must be positive and fit an int capacity");
	return static_cast<int>(bucketCount * bucketSize);
}

template<typename Node> std::size_t checkedPathfinderNodeCount(std::int64_t width, std::int64_t height, std::int64_t levels, std::int64_t capacity)
{
	if(width < 0 || height < 0 || levels < 0 || capacity <= 0)
		throw std::invalid_argument("Invalid pathfinder storage dimensions");
	if(!width || !height || !levels)
		return 0;
	const auto limit = std::vector<Node>().max_size();
	std::size_t count = 1;
	for(const auto factor : std::array<std::int64_t, 4>{width, height, levels, capacity})
	{
		if(static_cast<std::uint64_t>(factor) > limit / count)
			throw std::length_error("Pathfinder storage dimensions are too large");
		count *= static_cast<std::size_t>(factor);
	}
	return count;
}
}
