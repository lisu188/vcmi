"""Actual sacrifice handler with ordinary supported trades only.

Uses narrow hero/market/event stand-ins. These fixtures verify normal split
and large-count trades, not malformed request probes or a full server session.
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
#include <limits>
#include <map>
#include <string>
#include <vector>
using ui32=uint32_t;
using TQuantity=int32_t;
using TExpType=int64_t;
struct SlotID { int num; bool validSlot() const { return num>=0 && num<7; } bool operator<(SlotID other) const { return num<other.num; } };
struct Stack { int id; int count; int getId() const { return id; } };
struct CGHeroInstance
{
    int id=1;
    mutable std::map<SlotID,Stack> stacks;
    bool hasStackAtSlot(SlotID slot) const { return stacks.count(slot)!=0; }
    int getStackCount(SlotID slot) const { auto it=stacks.find(slot); return it==stacks.end()?0:it->second.count; }
    int stacksCount() const { return static_cast<int>(stacks.size()); }
    bool needsLastStack() const { return true; }
    const Stack & getStack(SlotID slot) const { return stacks.at(slot); }
    TExpType calculateXp(TExpType exp) const { return exp; }
};
enum class EMarketMode { CREATURE_EXP };
struct IMarket
{
    bool getOffer(int creature,int,int & units,int & xp,EMarketMode) const { units=1; xp=creature*10; return true; }
};
struct StackLocation { int hero; SlotID slot; };
enum class ChangeValueMode { RELATIVE };
struct CGameHandler
{
    CGHeroInstance * hero;
    TExpType gained=0;
    int changes=0, complaints=0;
    void complain(const std::string &) { ++complaints; }
    void giveExperience(const CGHeroInstance *,TExpType amount) { gained+=amount; }
    void changeStackCount(StackLocation location,TQuantity delta,ChangeValueMode)
    {
        auto & stack=hero->stacks.at(location.slot);
        stack.count+=delta;
        if(!stack.count) hero->stacks.erase(location.slot);
        ++changes;
    }
    bool sacrificeCreatures(const IMarket *,const CGHeroInstance *,const std::vector<SlotID> &,const std::vector<ui32> &);
};
#define COMPLAIN_RET(message) {complain(message); return false;}
'''
FIXTURES=r'''
int main()
{
    IMarket market;
    CGHeroInstance hero; hero.stacks={{{0},{1,10}},{{1},{2,5}}};
    CGameHandler game{&hero};
    assert(game.sacrificeCreatures(&market,&hero,{{0},{1}},{5,3}));
    assert(hero.getStackCount({0})==5 && hero.getStackCount({1})==2);
    assert(game.gained==110 && game.changes==2 && game.complaints==0);
    CGHeroInstance split; split.stacks={{{0},{1,10}}};
    CGameHandler splitGame{&split};
    assert(splitGame.sacrificeCreatures(&market,&split,{{0},{0}},{2,3}));
    assert(split.getStackCount({0})==5 && splitGame.gained==50 && splitGame.complaints==0);
    CGHeroInstance large; large.stacks={{{0},{1,2000001}}};
    CGameHandler largeGame{&large};
    assert(largeGame.sacrificeCreatures(&market,&large,{{0}},{2000000}));
    assert(large.getStackCount({0})==1 && largeGame.gained==20000000 && largeGame.complaints==0);
    std::cout<<"Ordinary creature-sacrifice fixtures passed\n";
}
'''
class SacrificeFixtures(unittest.TestCase):
    def test_actual_handler(self):
        source=(ROOT/'server/CGameHandler.cpp').read_text()
        start=source.index('bool CGameHandler::sacrificeCreatures(')
        opening=source.index('{',start)
        depth,end=1,opening+1
        while depth:
            if source[end]=='{': depth+=1
            elif source[end]=='}': depth-=1
            end+=1
        with tempfile.TemporaryDirectory(prefix='vcmi-valid-sacrifice-') as directory:
            cpp=Path(directory)/'sacrifice.cpp'
            binary=Path(directory)/'sacrifice'
            cpp.write_text('\n'.join([SUPPORT,source[start:end],FIXTURES]))
            subprocess.run([os.environ.get('CXX','g++'),'-std=c++20','-Wall','-Wextra','-Werror',
                            '-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                            str(cpp),'-o',str(binary)],check=True,timeout=60)
            result=subprocess.run([str(binary)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=20)
            self.assertEqual(result.returncode,0,result.stdout)
            self.assertIn('creature-sacrifice fixtures passed',result.stdout)
if __name__=='__main__':
    unittest.main()
