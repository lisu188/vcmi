/*
 * ScriptingHandler.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "ScriptingHandler.h"

namespace scripting
{

ScriptingHandler::ScriptingHandler() = default;
ScriptingHandler::~ScriptingHandler() = default;

void ScriptingHandler::addBackend(std::unique_ptr<Service> backend)
{
	if(backend)
		backends.push_back(std::move(backend));
}

void ScriptingHandler::installScripting(spells::effects::SpellEffectService * spellEffects)
{
	for(const auto & backend : backends)
		backend->installScripting(spellEffects);
}

std::unique_ptr<Pool> ScriptingHandler::createPoolInstance(const Environment * ENV) const
{
	std::vector<std::unique_ptr<Pool>> subPools;
	subPools.reserve(backends.size());
	for(const auto & backend : backends)
	{
		// A backend may legitimately produce no pool (e.g. disabled at runtime); skip it rather
		// than storing a null sub-pool that getContext would dereference.
		if(auto pool = backend->createPoolInstance(ENV))
			subPools.push_back(std::move(pool));
	}

	return std::make_unique<CompositePool>(std::move(subPools));
}

std::shared_ptr<Script> ScriptingHandler::loadScript(const std::string & scope, const std::string & source)
{
	for(const auto & backend : backends)
	{
		if(auto script = backend->loadScript(scope, source))
			return script;
	}
	return nullptr;
}

void ScriptingHandler::exportDocs(const boost::filesystem::path & outDir) const
{
	// Every backend writes into the shared output directory. With a single backend this matches
	// the historical behavior exactly; once a second backend is added each must emit distinct
	// filenames (or its own subdirectory) to avoid clobbering.
	for(const auto & backend : backends)
		backend->exportDocs(outDir);
}

CompositePool::CompositePool(std::vector<std::unique_ptr<Pool>> subPools)
	: subPools(std::move(subPools))
{
}

CompositePool::~CompositePool() = default;

std::shared_ptr<Context> CompositePool::getContext(const Script * script) const
{
	for(const auto & pool : subPools)
	{
		if(auto context = pool->getContext(script))
			return context;
	}
	return nullptr;
}

}
