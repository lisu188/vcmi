/*
 * LuaScriptModule.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/scripting/Service.h>

namespace spells::effects
{
class LuaSpellEffectFactory;
}

namespace scripting
{

class LuaScriptInstance;

/// Top-level Lua scripting service; owns script factories and creates script pools.
class DLL_LINKAGE LuaModule final : public Service
{
public:
	LuaModule();
	~LuaModule();

	void installScripting(spells::effects::SpellEffectService * spellEffects) override;

	std::unique_ptr<Pool> createPoolInstance(const Environment * ENV) const override;

	std::shared_ptr<Script> loadScript(const std::string & scope, const std::string & source) override;

	void exportDocs(const boost::filesystem::path & outDir) const override;

private:
	using ScriptPtr = std::shared_ptr<LuaScriptInstance>;
	using ScriptMap = std::map<std::string, ScriptPtr>;

	std::shared_ptr<spells::effects::LuaSpellEffectFactory> luaSpellEffects;

	/// General (non-spell-effect) scripts loaded via loadScript, keyed by identifier
	/// (scope + ':' + source). LuaModule is the owner-of-record; these outlive every pool and are
	/// re-registered into each pool created by createPoolInstance so that every game session
	/// gets a live LuaContext for them.
	ScriptMap generalScripts;
};
}
