"""Ordinary configured pathfinder capacities and custom template bounds.

The helper and template method are actual production code. This is a focused
arithmetic/representation check, not a map-generation or AI gameplay replay.
No malformed configuration/file probes or large allocations are performed.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def method(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    level = 1
    end = opening + 1
    while level:
        if text[end] == '{':
            level += 1
        elif text[end] == '}':
            level -= 1
        end += 1
    return text[start:end]


SUPPORT = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#include <boost/multiprecision/cpp_int.hpp>
struct int3 { int32_t x, y, z; };
class CRmgTemplate
{
public:
    int3 minSize, maxSize;
    bool matchesSize(const int3 &) const;
};
'''

FIXTURES = r'''
int main()
{
    CONFIGURED_CAPACITY_ASSERTIONS
    assert(NK2AI::checkedPathfinderNodeCount<uint64_t>(36, 36, 1, 32) == 41472);
    assert(NK2AI::checkedPathfinderNodeCount<uint64_t>(36, 36, 2, 32) == 82944);
    assert(NK2AI::checkedPathfinderNodeCount<uint64_t>(0, 36, 1, 32) == 0);

    CRmgTemplate ordinary{{36, 36, 1}, {252, 252, 2}};
    assert(ordinary.matchesSize(int3{72, 72, 2}));
    assert(!ordinary.matchesSize(int3{24, 24, 1}));
    // A loose positive custom upper bound must continue to allow an ordinary
    // giant map. The bound is compared only; no large map is allocated.
    CRmgTemplate looseUpper{{36, 36, 1}, {100000, 100000, 2}};
    assert(looseUpper.matchesSize(int3{252, 252, 2}));
    static_assert(std::numeric_limits<boost::multiprecision::int128_t>::digits >= 94);
    std::cout << "Configured capacities and ordinary/custom template fixtures passed\n";
}
'''


class CapacityVolumeFixtures(unittest.TestCase):
    def test_actual_capacity_helpers_and_template_method(self):
        config = (ROOT / 'config/ai/nk2ai/nk2ai-settings.json').read_text()
        counts = [int(value) for value in re.findall(r'"pathfinderBucketsCount"\s*:\s*(\d+)', config)]
        sizes = [int(value) for value in re.findall(r'"pathfinderBucketSize"\s*:\s*(\d+)', config)]
        self.assertEqual(len(counts), len(sizes))
        self.assertGreater(len(counts), 0)
        statements = '\n'.join('assert(NK2AI::checkedPathfinderCapacity(%d, %d) == %d);' % (count, size, count * size)
                               for count, size in zip(counts, sizes))
        header = (ROOT / 'AI/Nullkiller2/Engine/PathfinderCapacity.h').read_text().replace('#pragma once', '')
        template = (ROOT / 'lib/rmg/CRmgTemplate.cpp').read_text()
        program = '\n'.join([SUPPORT, header, method(template, 'bool CRmgTemplate::matchesSize(const int3 & value) const'),
                             FIXTURES.replace('CONFIGURED_CAPACITY_ASSERTIONS', statements)])
        with tempfile.TemporaryDirectory(prefix='vcmi-valid-capacity-') as directory:
            cpp = Path(directory) / 'capacity.cpp'
            binary = Path(directory) / 'capacity'
            cpp.write_text(program)
            subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                            str(cpp), '-o', str(binary)], check=True, timeout=90)
            result = subprocess.run([str(binary)], text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=20)
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertIn('template fixtures passed', result.stdout)


if __name__ == '__main__':
    unittest.main()
