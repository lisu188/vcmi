"""Compile actual production code with small, ordinary valid-data fixtures.

SDL and squish are narrow interface stand-ins: these fixtures exercise loader
layout, cropping, cache reuse and map serialization, not decompressor fidelity
or a full VCMI/SDL build. No retail assets, hostile files or large allocations.
"""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def source_body(path):
    return '\n'.join(line for line in (ROOT / path).read_text().splitlines()
                     if not line.startswith('#include') and line != '#pragma once')


SUPPORT = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
using ui8 = uint8_t;
using si64 = int64_t;
struct Rect { int x, y, w, h; };
struct int3 { int32_t x, y, z; };
struct SDL_Surface { void * pixels; int pitch, w, h; };
constexpr int SDL_PIXELFORMAT_RGBA32 = 1;
static int decompressions = 0;
SDL_Surface * SDL_CreateRGBSurfaceWithFormat(int, int w, int h, int, int)
{
    auto * result = new SDL_Surface{};
    result->w = w;
    result->h = h;
    result->pitch = w * 4 + 4; // Exercise a destination pitch with padding.
    result->pixels = new uint8_t[size_t(result->pitch) * h]{};
    return result;
}
void freeSurface(SDL_Surface * surface)
{
    delete[] static_cast<uint8_t *>(surface->pixels);
    delete surface;
}
namespace squish
{
enum { kDxt1 = 1, kDxt5 = 2 };
void DecompressImage(uint8_t * out, int w, int h, const void *, int)
{
    ++decompressions;
    for(size_t i = 0; i < size_t(w) * h * 4; ++i)
        out[i] = static_cast<uint8_t>(i % 251);
}
}
class CInputStream
{
    std::vector<uint8_t> bytes;
    size_t position = 0;
public:
    explicit CInputStream(std::vector<uint8_t> value) : bytes(std::move(value)) {}
    si64 tell() const { return static_cast<si64>(position); }
    si64 getSize() const { return static_cast<si64>(bytes.size()); }
    si64 read(ui8 * output, si64 requested)
    {
        assert(requested >= 0);
        const auto count = std::min(static_cast<size_t>(requested), bytes.size() - position);
        std::copy_n(bytes.data() + position, count, output);
        position += count;
        return static_cast<si64>(count);
    }
};
struct MemoryArchive
{
    bool saving;
    std::vector<int64_t> & values;
    size_t position = 0;
    template<typename T> MemoryArchive & operator&(T & value)
    {
        if(saving) values.push_back(value);
        else { assert(position < values.size()); value = static_cast<T>(values[position++]); }
        return *this;
    }
};
'''

FIXTURES = r'''
void put32(std::vector<uint8_t> & data, size_t offset, uint32_t value)
{
    for(size_t byte = 0; byte < 4; ++byte)
        data[offset + byte] = static_cast<uint8_t>(value >> (byte * 8));
}
std::vector<uint8_t> dds(uint32_t w, uint32_t h, uint32_t format, size_t compressedBytes)
{
    std::vector<uint8_t> data(4 + 124 + compressedBytes);
    put32(data, 0, FOURCC('D','D','S',' '));
    put32(data, 4, 124);
    put32(data, 12, h);
    put32(data, 16, w);
    put32(data, 76, 32);
    put32(data, 84, format);
    return data;
}
void assertPixels(SDL_Surface * image, int sourceWidth, int x, int y, int width, int height)
{
    assert(image && image->w == width && image->h == height);
    auto * pixels = static_cast<uint8_t *>(image->pixels);
    for(int row = 0; row < height; ++row)
        for(int column = 0; column < width * 4; ++column)
            assert(pixels[size_t(row) * image->pitch + column] ==
                   ((size_t(row + y) * sourceWidth + x) * 4 + column) % 251);
}
int main()
{
    DdsFormat loader;
    CInputStream dxt1(dds(4, 4, FOURCC('D','X','T','1'), 8));
    auto * first = loader.load(&dxt1, "valid-dxt1");
    assertPixels(first, 4, 0, 0, 4, 4);
    assert(dxt1.tell() == 136 && decompressions == 1);
    freeSurface(first);

    Rect region{1, 1, 2, 2};
    auto * cached = loader.load(nullptr, "valid-dxt1", &region);
    assertPixels(cached, 4, 1, 1, 2, 2);
    assert(decompressions == 1);
    freeSurface(cached);

    CInputStream dxt5(dds(5, 7, FOURCC('D','X','T','5'), 64));
    auto * second = loader.load(&dxt5, "valid-dxt5");
    assertPixels(second, 5, 0, 0, 5, 7);
    assert(dxt5.tell() == 192 && decompressions == 2);
    freeSurface(second);

    MapTilesStorage<int> original(int3{3, 2, 2});
    int next = 0;
    for(int z = 0; z < 2; ++z)
        for(int x = 0; x < 3; ++x)
            for(int y = 0; y < 2; ++y)
                original[int3{x, y, z}] = next++;
    int expected = 0;
    for(int value : original) assert(value == expected++);
    assert(expected == 12);
    std::vector<int64_t> wire;
    MemoryArchive save{true, wire};
    original.serialize(save);
    assert((wire == std::vector<int64_t>{0, 2, 3, 2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}));
    MapTilesStorage<int> roundTrip;
    MemoryArchive load{false, wire};
    roundTrip.serialize(load);
    assert((roundTrip[int3{2, 1, 1}] == 11));
    MapTilesStorage<int> empty(int3{0, 0, 0});
    assert(empty.begin() == empty.end());
    std::cout << "Valid DDS loading/crop/cache and map-layout/serialization fixtures passed\n";
}
'''


class ImageStorageFixtures(unittest.TestCase):
    def test_valid_production_code_paths(self):
        program = '\n'.join([
            SUPPORT,
            source_body('client/render/hdEdition/DdsFormat.h'),
            source_body('client/render/hdEdition/DdsFormat.cpp'),
            source_body('lib/mapping/MapTilesStorage.h'),
            FIXTURES,
        ])
        with tempfile.TemporaryDirectory(prefix='vcmi-valid-image-storage-') as directory:
            source = Path(directory) / 'valid-fixtures.cpp'
            executable = Path(directory) / 'valid-fixtures'
            source.write_text(program)
            command = [os.environ.get('CXX', 'g++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
                       '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                       str(source), '-o', str(executable)]
            subprocess.run(command, check=True, timeout=60)
            result = subprocess.run([str(executable)], text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=20)
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertIn('fixtures passed', result.stdout)


if __name__ == '__main__':
    unittest.main()
