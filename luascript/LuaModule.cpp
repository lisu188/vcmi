/*
 * LuaScriptModule.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "LuaModule.h"

#include "LuaScriptInstance.h"
#include "LuaScriptPool.h"
#include "LuaSpellEffect.h"

#include "api/DocExport.h"

#include "../lib/GameLibrary.h"
#include "../lib/spells/effects/SpellEffectService.h"

namespace scripting
{

LuaModule::LuaModule() = default;
LuaModule::~LuaModule() = default;

void LuaModule::installScripting(spells::effects::SpellEffectService * spellEffects)
{
	luaSpellEffects = std::make_shared<spells::effects::LuaSpellEffectFactory>(*this);
	spellEffects->registerFactory("lua", luaSpellEffects);
}

std::unique_ptr<Pool> LuaModule::createPoolInstance(const Environment * ENV) const
{
	auto result = std::make_unique<LuaScriptPool>(*this, ENV);
	luaSpellEffects->registerScripts(result.get());
	for(const auto & [id, script] : generalScripts)
		result->registerScript(script.get());
	return result;
}

std::shared_ptr<Script> LuaModule::loadScript(const std::string & scope, const std::string & source)
{
	// Extension dispatch: this backend only handles .lua sources; anything else is another
	// backend's job (composite ScriptingHandler will try the next backend on nullptr).
	if(source.size() < 4 || source.compare(source.size() - 4, 4, ".lua") != 0)
		return nullptr;

	std::string id = scope + ':' + source;

	auto it = generalScripts.find(id);
	if(it != generalScripts.end())
		return it->second; // already loaded (declared twice) - share the same instance

	// General scripts have no patch layers; the ctor reads the base source from VFS SCRIPTS/.
	auto script = std::make_shared<LuaScriptInstance>(*this, scope, source,
		std::vector<std::pair<std::string, std::string>>{});

	if(script->layers.empty())
	{
		// Base source failed to load (LuaScriptInstance logs the VFS miss); do not register.
		logMod->error("Lua backend failed to load script '%s'", id);
		return nullptr;
	}

	generalScripts[id] = script;
	return script;
}

void LuaModule::exportDocs(const boost::filesystem::path & outDir) const
{
	api::exportLuaApiDocs(outDir);
}

}
