"""Actual spell-list rebuilding and slicing with ordinary service/UI stand-ins.

Exercises repeated normal rebuilds and page layouts, not native GUI rendering,
physical input or a full VCMI client. No malformed file or exploit probes.
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
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
enum class SpellSchool { ANY, AIR, FIRE };
struct CSpell
{
    int id;
    std::set<SpellSchool> schools{SpellSchool::AIR};
    bool isCombat() const { return false; }
    bool isSpecial() const { return false; }
    bool isCreatureAbility() const { return false; }
    std::string getNameTranslated() const { return "Spell" + std::to_string(id); }
    template<typename F> void forEachSchool(F visitor) const
    {
        bool stop = false;
        for(auto school : schools) { visitor(school, stop); if(stop) break; }
    }
};
struct SpellbookSpellSorter { bool operator()(const CSpell * a, const CSpell * b) const { return a->id < b->id; } };
struct SpellHandler { std::vector<std::unique_ptr<CSpell>> objects; };
struct SchoolHandler { std::vector<SpellSchool> getAllObjects() const { return {SpellSchool::ANY, SpellSchool::AIR, SpellSchool::FIRE}; } };
struct Library { SpellHandler * spellh; SchoolHandler * spellSchoolHandler; };
Library * LIBRARY;
struct Hero { bool canCastThisSpell(const CSpell *) const { return true; } };
struct ShowAll { bool isSelected() const { return false; } };
struct SearchBox { std::string text; std::string getText() const { return text; } };
namespace TextOperations { bool textSearchSimilarityScore(const std::string & query, const std::string & text) { return text.find(query) != std::string::npos; } }
struct SpellArea { const CSpell * spell = nullptr; void setSpell(const CSpell * value) { spell = value; } };
class CSpellWindow
{
public:
    Hero * myHero;
    ShowAll * showAllSpells;
    SearchBox * searchBox = nullptr;
    bool onSpellSelect = false, openOnBattleSpells = false, battleSpellsOnly = false;
    int spellsPerPage = 12, currentPage = 0;
    SpellSchool selectedTab = SpellSchool::ANY;
    std::vector<const CSpell *> mySpells;
    std::map<SpellSchool, int> sitesPerTabAdv, sitesPerTabBattle;
    std::vector<std::unique_ptr<SpellArea>> spellAreas;
    void processSpells();
    void computeSpellsPerArea();
    void redraw() {}
};
'''

FIXTURES = r'''
int main()
{
    SpellHandler spells;
    for(int id = 0; id < 240; ++id) spells.objects.push_back(std::make_unique<CSpell>(CSpell{id}));
    SchoolHandler schools;
    Library library{&spells, &schools}; LIBRARY = &library;
    Hero hero; ShowAll showAll;
    CSpellWindow window; window.myHero = &hero; window.showAllSpells = &showAll;
    for(int slot = 0; slot < 24; ++slot) window.spellAreas.push_back(std::make_unique<SpellArea>());
    for(int rebuild = 0; rebuild < 3; ++rebuild)
    {
        window.processSpells();
        assert(window.mySpells.size() == 240);
        assert(window.sitesPerTabAdv[SpellSchool::ANY] == 20);
        assert(window.sitesPerTabAdv[SpellSchool::AIR] == 21);
    }
    window.currentPage = 19;
    window.computeSpellsPerArea();
    for(int slot = 0; slot < 12; ++slot) assert(window.spellAreas[slot]->spell->id == 228 + slot);
    window.selectedTab = SpellSchool::AIR; window.currentPage = 0;
    window.computeSpellsPerArea();
    assert(!window.spellAreas[0]->spell && !window.spellAreas[1]->spell);
    for(int slot = 2; slot < 12; ++slot) assert(window.spellAreas[slot]->spell->id == slot - 2);
    window.currentPage = 20;
    window.computeSpellsPerArea();
    assert(window.spellAreas[0]->spell->id == 238 && window.spellAreas[1]->spell->id == 239);
    for(int slot = 2; slot < 12; ++slot) assert(!window.spellAreas[slot]->spell);
    // Ordinary filtered list rebuilds must recompute page counts from scratch.
    spells.objects.resize(24);
    window.processSpells();
    assert(window.sitesPerTabAdv[SpellSchool::ANY] == 2);
    assert(window.sitesPerTabAdv[SpellSchool::AIR] == 3);
    window.selectedTab = SpellSchool::ANY; window.currentPage = 1;
    window.computeSpellsPerArea();
    for(int slot = 0; slot < 12; ++slot) assert(window.spellAreas[slot]->spell->id == 12 + slot);
    spells.objects.clear(); window.processSpells(); window.currentPage = 0;
    window.computeSpellsPerArea();
    for(int slot = 0; slot < 12; ++slot) assert(!window.spellAreas[slot]->spell);
    std::cout << "Ordinary repeated rebuild and spell-page layout fixtures passed\n";
}
'''


class SpellPageFixtures(unittest.TestCase):
    def test_actual_rebuild_and_page_methods(self):
        source = (ROOT / 'client/windows/CSpellWindow.cpp').read_text()
        header = (ROOT / 'client/windows/SpellPageRange.h').read_text().replace('#pragma once', '')
        program = '\n'.join([SUPPORT, header, method(source, 'void CSpellWindow::processSpells()'),
                             method(source, 'void CSpellWindow::computeSpellsPerArea()'), FIXTURES])
        with tempfile.TemporaryDirectory(prefix='vcmi-valid-spell-pages-') as directory:
            cpp = Path(directory) / 'pages.cpp'
            binary = Path(directory) / 'pages'
            cpp.write_text(program)
            subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            '-Wno-sign-compare', '-Wno-unused-parameter', '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                            str(cpp), '-o', str(binary)], check=True, timeout=60)
            result = subprocess.run([str(binary)], text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=20)
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertIn('layout fixtures passed', result.stdout)


if __name__ == '__main__':
    unittest.main()
