/*
 * scripting/Service.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/Environment.h>

#include <boost/filesystem/path.hpp>

class JsonNode;

namespace spells::effects
{
    class SpellEffectService;
}

namespace scripting
{

using BattleCb = Environment::BattleCb;
using GameCb = Environment::GameCb;

class DLL_LINKAGE Context
{
public:
	virtual ~Context() = default;

	/// Invokes a named function the script exposes, passing a single JSON parameter object, and
	/// returns the script's result as JSON. This is the data-only entry point for general engine
	/// hooks: no live engine objects cross the neutral interface. The caller passes identifiers
	/// (player, day, objectID, heroID, battleID, ...) inside `parameters`, and the script reaches
	/// the world through the GAME/LIBRARY API already injected into its context at creation.
	/// Returns an empty (null) JsonNode if the script defines no such function or the call fails.
	///
	/// This exists as a plain virtual - rather than the variadic template callMethod on a concrete
	/// backend - precisely so the backend-neutral engine can call it without knowing the backend
	/// type. A variadic member template cannot be virtual.
	virtual JsonNode callGlobal(const std::string & functionName, const JsonNode & parameters) = 0;
};

class DLL_LINKAGE Script
{
public:
	virtual ~Script() = default;

	virtual std::string getIdentifier() const = 0;
};

class DLL_LINKAGE Pool
{
public:
	virtual ~Pool() = default;

	/// Returns the live context for a script owned by this pool, or nullptr if this pool does not
	/// own the given script. Returning nullptr (rather than throwing) lets a composite pool query
	/// several backend pools in turn and pick the one that owns the script.
	virtual std::shared_ptr<Context> getContext(const Script * script) const = 0;
};

class DLL_LINKAGE Service
{
public:
	virtual ~Service() = default;

	virtual void installScripting(spells::effects::SpellEffectService * spellEffects) = 0;

	virtual std::unique_ptr<Pool> createPoolInstance(const Environment * ENV) const = 0;

	/// Loads a single general (non-spell-effect) script from the given mod scope and source path
	/// (a VFS path under SCRIPTS/, e.g. "events/onTurn.lua"), returning a Script handle owned by
	/// this backend, or nullptr if this backend does not handle that source. Backends dispatch on
	/// the file extension (".lua" -> Lua, ".py" -> Python); a composite Service forwards to each
	/// backend and takes the first non-null result. The returned Script must stay alive for the
	/// whole game session - the backend retains ownership and re-registers it into every pool it
	/// creates (see createPoolInstance). Returns nullptr if the source cannot be loaded.
	virtual std::shared_ptr<Script> loadScript(const std::string & scope, const std::string & source) = 0;

	/// Writes Markdown and Lua Language Server reference files describing every exposed API type
	/// into the given output directory. Used by `vcmiserver --export-lua-docs <path>` to keep
	/// the modder-facing scripting reference in sync with the host bindings.
	virtual void exportDocs(const boost::filesystem::path & outDir) const = 0;
};

}
