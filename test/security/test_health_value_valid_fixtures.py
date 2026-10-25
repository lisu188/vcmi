"""Exercise actual health arithmetic with ordinary representable stack values.

The unit owner's interface and state-application sink are narrow stand-ins;
this verifies wide health values, not a full combat engine or gameplay replay.
"""
from pathlib import Path
import os
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
#include <stdexcept>
namespace vstd
{
template<typename T, typename U> void amax(T & value, U minimum)
{
    if(value < minimum) value = minimum;
}
}
struct Owner
{
    uint32_t maxHealth;
    uint32_t getMaxHealth() const { return maxHealth; }
};
class CHealth
{
public:
    Owner * owner;
    int32_t firstHPleft, fullUnits, resurrected;
    int64_t appliedTotal = -1;
    int64_t available() const;
    void takeResurrected();
    void setFromTotal(int64_t value) { appliedTotal = value; }
};
'''

FIXTURES = r'''
int main()
{
    Owner ordinary{123};
    CHealth small{&ordinary, 17, 10, 2};
    assert(small.available() == 1247);
    small.takeResurrected();
    assert(small.appliedTotal == 1001 && small.resurrected == 0);

    // These positive int32 counts and uint32 health values fit the public
    // interfaces. Their exact health totals require the declared int64 result.
    Owner large{5000};
    CHealth many{&large, 1000, 1000000, 100000};
    assert(many.available() == INT64_C(5000001000));
    many.takeResurrected();
    assert(many.appliedTotal == INT64_C(4500001000));
    assert(many.resurrected == 0);

    CHealth noResurrection{&large, 1000, 1000000, 0};
    noResurrection.takeResurrected();
    assert(noResurrection.appliedTotal == -1);
    std::cout << "Valid ordinary and high-positive-count health fixtures passed\n";
}
'''


class HealthValueFixtures(unittest.TestCase):
    def test_actual_health_methods_keep_wide_values(self):
        source = (ROOT / 'lib/battle/CUnitState.cpp').read_text()
        program = '\n'.join([SUPPORT, method(source, 'int64_t CHealth::available() const'),
                             method(source, 'void CHealth::takeResurrected()'), FIXTURES])
        with tempfile.TemporaryDirectory(prefix='vcmi-valid-health-') as directory:
            cpp = Path(directory) / 'health.cpp'
            binary = Path(directory) / 'health'
            cpp.write_text(program)
            subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                            str(cpp), '-o', str(binary)], check=True, timeout=60)
            result = subprocess.run([str(binary)], text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=20)
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertIn('health fixtures passed', result.stdout)


if __name__ == '__main__':
    unittest.main()
