"""Actual upgrade-value constructor with ordinary valid army/mod records.

No full AI gameplay or malformed-input probing. Cost and creature lookup use
narrow stand-ins; the production subtraction/product/assignment are compiled.
"""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]

SUPPORT=r'''
#include <cassert>
#include <cstdint>
#include <iostream>
struct TResources
{
    int gold;
    TResources operator-(TResources other) const { return {gold-other.gold}; }
    TResources operator*(int count) const { return {gold*count}; }
};
struct Creature
{
    int32_t aiValue;
    int gold;
    int32_t getAIValue() const { return aiValue; }
    TResources getFullRecruitCost() const { return {gold}; }
};
struct CreatureID
{
    const Creature * creature;
    const Creature * toCreature() const { return creature; }
};
'''
FIXTURES=r'''
int main()
{
    Creature initial{1000,1}, upgraded{5000,2};
    StackUpgradeInfo ordinary({&initial},{&upgraded},100);
    assert(ordinary.upgradeValue==400000);
    assert(ordinary.cost.gold==100);
    StackUpgradeInfo large({&initial},{&upgraded},2000000);
    assert(large.upgradeValue==8000000000ULL);
    assert(large.cost.gold==2000000);
    Creature lowerAllowedUpgrade{500,2};
    StackUpgradeInfo lower({&initial},{&lowerAllowedUpgrade},10);
    assert(lower.upgradeValue==static_cast<uint64_t>(-5000));
    std::cout<<"Ordinary upgrade-value fixtures passed\n";
}
'''

class UpgradeValueFixtures(unittest.TestCase):
    def test_actual_constructor(self):
        source=(ROOT/'AI/Nullkiller2/Analyzers/ArmyManager.cpp').read_text()
        start=source.index('class StackUpgradeInfo')
        opening=source.index('{',start)
        depth,end=1,opening+1
        while depth:
            if source[end]=='{': depth+=1
            elif source[end]=='}': depth-=1
            end+=1
        declaration=source[start:end]+';'
        with tempfile.TemporaryDirectory(prefix='vcmi-valid-upgrade-value-') as directory:
            cpp=Path(directory)/'upgrade.cpp'
            binary=Path(directory)/'upgrade'
            cpp.write_text('\n'.join([SUPPORT,declaration,FIXTURES]))
            subprocess.run([os.environ.get('CXX','g++'),'-std=c++20','-Wall','-Wextra','-Werror',
                            '-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                            str(cpp),'-o',str(binary)],check=True,timeout=60)
            result=subprocess.run([str(binary)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=20)
            self.assertEqual(result.returncode,0,result.stdout)
            self.assertIn('upgrade-value fixtures passed',result.stdout)

if __name__=='__main__':
    unittest.main()
