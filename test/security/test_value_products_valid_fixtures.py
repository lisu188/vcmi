"""Ordinary rebirth and damage-label source fixtures.

Compiles the actual prepareAttacked method and damage-label source fragments
with narrow normal game/UI stand-ins. This is not a full combat or GUI replay.
No malformed-input probes are executed.
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
    depth, end = 1, opening + 1
    while depth:
        if text[end] == '{': depth += 1
        elif text[end] == '}': depth -= 1
        end += 1
    return text[start:end]

SUPPORT = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
using std::floor;
enum class BonusType { REBIRTH, SIEGE_WEAPON, PRIMARY_SKILL };
enum class BonusCustomSubtype { rebirthSpecial };
enum class BonusSource { ARTIFACT, HERO_BASE_SKILL };
enum class PrimarySkill { ATTACK };
struct BonusSubtypeID { explicit BonusSubtypeID(PrimarySkill) {} };
struct DummySelector
{
    DummySelector Or(DummySelector) const { return {}; }
    DummySelector And(DummySelector) const { return {}; }
};
namespace Selector
{
DummySelector sourceTypeSel(BonusSource) { return {}; }
DummySelector typeSubtype(BonusType, BonusSubtypeID) { return {}; }
}
enum class EHealLevel { RESURRECT };
enum class EHealPower { PERMANENT };
struct UnitChanges { enum class EOperation { UPDATE }; };
struct BattleStackAttacked
{
    enum { CLONE_KILLED=1, KILLED=2, REBIRTH=4 };
    int flags=0;
    int32_t killedAmount=0;
    int64_t damageAmount=0;
    struct { int data=0; int64_t healthDelta=0; int id=0; UnitChanges::EOperation operation{}; } newState;
};
namespace vstd
{
struct RNG { int nextInt(int, int) { return 0; } };
template<class T> void amax(T & a, T b) { a=std::max(a,b); }
}
namespace battle
{
struct Counter { int value=1; void use() { --value; } void use(int amount) { value-=amount; } int available() const { return value; } };
struct CUnitState
{
    int32_t count, baseCount, rebirth;
    uint32_t hp;
    int64_t receivedHealing=0;
    Counter casts, counterAttacks;
    CUnitState(int32_t count, uint32_t hp, int32_t rebirth):count(count),baseCount(count),rebirth(rebirth),hp(hp) {}
    int32_t getCount() const { return count; }
    void damage(int64_t & amount) { assert(amount==static_cast<int64_t>(count)*hp); count=0; }
    bool alive() const { return count>0; }
    bool isClone() const { return false; }
    int valOfBonuses(BonusType) const { return rebirth; }
    bool canCast() const { return casts.value>0; }
    int32_t unitBaseAmount() const { return baseCount; }
    bool hasBonusOfType(BonusType, BonusCustomSubtype) const { return false; }
    uint32_t getMaxHealth() const { return hp; }
    void heal(int64_t & amount, EHealLevel, EHealPower) { receivedHealing=amount; count=static_cast<int32_t>(amount/hp); }
    int save() { return count; }
    int unitId() const { return 1; }
};
}
struct CStack
{
    bool siege=true;
    int bonus=0, minDamage=0, maxDamage=0;
    bool hasBonusOfType(BonusType) const { return siege; }
    int valOfBonuses(DummySelector) const { return bonus; }
    bool isShooter() const { return true; }
    int getMaxDamage(bool) const { return maxDamage; }
    static void prepareAttacked(BattleStackAttacked &, vstd::RNG &, const std::shared_ptr<battle::CUnitState> &);
};
struct DamageNode { int minDamage; int getMinDamage(bool) const { return minDamage; } };
struct Info { DamageNode * stackNode; };
struct Parent { Info * info; };
enum class EStat { DAMAGE };
std::pair<int64_t,int64_t> labels;
void addStatLabel(EStat, int64_t low, int64_t high) { labels={low,high}; }
'''

FIXTURES = r'''
int main()
{
    for(auto fixture : {std::pair{100000,100}, std::pair{200000,50}})
    {
        auto state=std::make_shared<battle::CUnitState>(fixture.first,100000,fixture.second);
        BattleStackAttacked result;
        result.damageAmount=static_cast<int64_t>(fixture.first)*state->hp;
        vstd::RNG random;
        CStack::prepareAttacked(result,random,state);
        assert(result.flags==(BattleStackAttacked::KILLED|BattleStackAttacked::REBIRTH));
        assert(result.killedAmount==fixture.first);
        assert(state->receivedHealing==10000000000LL);
        assert(state->count==100000);
        assert(result.newState.data==100000);
        assert(state->casts.value==0);
    }
    CStack normal; normal.siege=false; normal.minDamage=15; normal.maxDamage=30;
    checkDisplay(normal,15,30);
    CStack siege; siege.bonus=100000; siege.minDamage=100000; siege.maxDamage=200000;
    checkDisplay(siege,10000100000LL,20000200000LL);
    std::cout<<"Ordinary high-count rebirth and damage-label fixtures passed\n";
}
'''

class ValueProductFixtures(unittest.TestCase):
    def test_actual_rebirth_and_damage_source(self):
        stack = (ROOT/'lib/CStack.cpp').read_text()
        window = (ROOT/'client/windows/CCreatureWindow.cpp').read_text()
        multiplier_start = window.index('int64_t dmgMultiply = 1;')
        multiplier_end = window.index('\n\tstatic const std::array', multiplier_start)
        multiplier = window[multiplier_start:multiplier_end]
        damage_line = next(line for line in window.splitlines()
                           if 'addStatLabel(EStat::DAMAGE' in line and 'dmgMultiply' in line)
        display = '\n'.join([
            'void checkDisplay(CStack & battle, int64_t low, int64_t high) {',
            'const CStack * battleStack=&battle;',
            'DamageNode node{battle.minDamage}; Info info{&node}; Parent p{&info}; Parent * parent=&p;',
            multiplier, damage_line, 'assert(labels==std::make_pair(low,high));', '}'])
        program='\n'.join([SUPPORT,method(stack,'void CStack::prepareAttacked(BattleStackAttacked & bsa, vstd::RNG & rand, const std::shared_ptr<battle::CUnitState> & customState)'),display,FIXTURES])
        with tempfile.TemporaryDirectory(prefix='vcmi-valid-value-products-') as directory:
            cpp=Path(directory)/'values.cpp'
            binary=Path(directory)/'values'
            cpp.write_text(program)
            subprocess.run([os.environ.get('CXX','g++'),'-std=c++20','-Wall','-Wextra','-Werror',
                            '-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                            str(cpp),'-o',str(binary)],check=True,timeout=60)
            result=subprocess.run([str(binary)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=20)
            self.assertEqual(result.returncode,0,result.stdout)
            self.assertIn('damage-label fixtures passed',result.stdout)

if __name__=='__main__':
    unittest.main()
