/*
 * LuaScriptPool.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "LuaScriptPool.h"

#include "LuaScriptInstance.h"
#include "LuaContext.h"

#include "../lib/json/JsonNode.h"

namespace scripting
{

LuaScriptPool::LuaScriptPool(const LuaModule & luaModule, const Environment * ENV)
	: env(ENV)
{
}

void LuaScriptPool::registerScript(const LuaScriptInstance * script)
{
	auto context = script->createContext(env);
	cache[script] = context;
	context->initialize();
}

std::shared_ptr<Context> LuaScriptPool::getContext(const Script * script) const
{
	auto it = cache.find(script);
	if(it == cache.end())
		return nullptr; // script is not owned by this pool (e.g. belongs to another backend)
	return it->second;
}
}
