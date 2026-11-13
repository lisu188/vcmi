/*
 * MapTilesStorage.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../int3.h"

#include <limits>
#include <stdexcept>

template<typename DataType>
class MapTilesStorage
{
	using TInternalContainer = std::vector<DataType>;
	TInternalContainer storage;
	int3 dimensions = {0,0,0};

	static size_t checkedTileCount(const int3 & size)
	{
		if(size.x < 0 || size.y < 0 || size.z < 0)
			throw std::length_error("Negative map storage dimensions");
		if(!size.x || !size.y || !size.z)
			return 0;
		// The map interfaces use signed 32-bit tile indices. Retain that
		// representation limit instead of silently wrapping an allocation.
		const auto limit = static_cast<size_t>(std::numeric_limits<int32_t>::max());
		size_t count = static_cast<size_t>(size.x);
		if(size.y && count > limit / size.y)
			throw std::length_error("Map storage dimensions are too large");
		count *= size.y;
		if(size.z && count > limit / size.z)
			throw std::length_error("Map storage dimensions are too large");
		return count * size.z;
	}

	size_t getTileIndex(const int3 & tile) const
	{
		assert(tile.x >= 0 && tile.x < dimensions.x);
		assert(tile.y >= 0 && tile.y < dimensions.y);
		assert(tile.z >= 0 && tile.z < dimensions.z);
		return size_t(tile.z) * dimensions.x * dimensions.y + size_t(tile.x) * dimensions.y + tile.y;
	}

public:
	using const_reference = typename TInternalContainer::const_reference;
	using value_type = typename TInternalContainer::value_type;

	using const_iterator = typename TInternalContainer::const_iterator;
	using iterator = typename TInternalContainer::iterator;

	MapTilesStorage() = default;
	MapTilesStorage(int3 dimensions) :
		storage(checkedTileCount(dimensions)),
		dimensions(dimensions)
	{}

	const_iterator begin() const { return storage.begin(); }
	const_iterator end() const { return storage.end(); }
	iterator begin() { return storage.begin(); }
	iterator end() { return storage.end(); }

	const DataType & operator[] (const int3 & tile) const { return storage[getTileIndex(tile)];}
	DataType & operator[](const int3 & tile) { return storage[getTileIndex(tile)];}

	template <typename Handler>
	void serialize(Handler &h)
	{
		uint32_t lengthUnused = 0;
		h & lengthUnused;
		auto serializedDimensions = dimensions;
		h & serializedDimensions.z;
		h & serializedDimensions.x;
		h & serializedDimensions.y;
		if (!h.saving)
		{
			storage.resize(checkedTileCount(serializedDimensions));
			dimensions = serializedDimensions;
		}

		for (auto & element : storage)
			h & element;
	}
};

