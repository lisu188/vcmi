/*
 * PythonScriptPool.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "PythonScriptPool.h"

#include "PythonContext.h"
#include "PythonScriptInstance.h"

namespace scripting
{

PythonScriptPool::PythonScriptPool(const PythonModule & pythonModule, const Environment * ENV)
	: env(ENV)
{
}

void PythonScriptPool::registerScript(const PythonScriptInstance * script)
{
	auto context = script->createContext(env);
	cache[script] = context;
	context->initialize();
}

std::shared_ptr<Context> PythonScriptPool::getContext(const Script * script) const
{
	auto it = cache.find(script);
	if(it == cache.end())
		return nullptr; // script is not owned by this pool (e.g. belongs to another backend)
	return it->second;
}

}
