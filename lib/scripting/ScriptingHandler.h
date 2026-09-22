/*
 * ScriptingHandler.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/scripting/Service.h>

namespace scripting
{

/// Composite scripting service that aggregates several backend Service implementations
/// (e.g. Lua and Python) behind the single scripting::Service interface that the engine expects.
///
/// GameLibrary owns exactly one scripting::Service and CGameState owns exactly one Pool; this
/// class lets multiple language backends coexist without changing that contract. Backends are
/// registered with addBackend() at startup; every Service call is forwarded to all of them, and
/// createPoolInstance() returns a composite Pool that dispatches getContext() to the owning backend.
class DLL_LINKAGE ScriptingHandler final : public Service
{
public:
	ScriptingHandler();
	~ScriptingHandler();

	/// Adds a backend. Order is preserved; getContext() queries backends in registration order.
	void addBackend(std::unique_ptr<Service> backend);

	void installScripting(spells::effects::SpellEffectService * spellEffects) override;

	std::unique_ptr<Pool> createPoolInstance(const Environment * ENV) const override;

	std::shared_ptr<Script> loadScript(const std::string & scope, const std::string & source) override;

	void exportDocs(const boost::filesystem::path & outDir) const override;

private:
	std::vector<std::unique_ptr<Service>> backends;
};

/// Pool that fans getContext() out across one sub-pool per backend, returning the first sub-pool
/// that owns the requested script. Created by ScriptingHandler::createPoolInstance.
class DLL_LINKAGE CompositePool final : public Pool
{
public:
	explicit CompositePool(std::vector<std::unique_ptr<Pool>> subPools);
	~CompositePool();

	std::shared_ptr<Context> getContext(const Script * script) const override;

private:
	std::vector<std::unique_ptr<Pool>> subPools;
};

}
