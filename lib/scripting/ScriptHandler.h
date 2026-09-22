/*
 * ScriptHandler.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../IHandlerBase.h"

class JsonNode;

namespace scripting
{
class Script;
}

/// Engine event kinds a general (non-spell-effect) script may hook.
/// Kept intentionally small; extend alongside config/schemas/script.json "implements" enum.
enum class ScriptEventKind
{
	ON_PLAYER_TURN_START, ///< fired server-side from NewTurnProcessor::onPlayerTurnStarted
	// ON_OBJECT_VISIT,   ///< milestone 3: CGObjectInstance::onHeroVisit
	// ON_BATTLE_START,   ///< milestone 3: BattleProcessor::setupBattle
};

/// Content handler for the "scripts" mod content type: general engine-event scripts declared with
/// config/schemas/script.json (fields: source, implements). Modeled on SpellEffectHandler
/// (a plain IHandlerBase, NOT the index-addressable CHandlerBase template) because scripts are not
/// entities and are not referenced by numeric ID.
///
/// Ownership: this handler holds a shared_ptr to every loaded scripting::Script for the whole game
/// lifetime and indexes them by ScriptEventKind. It does NOT register scripts into per-session pools;
/// the owning scripting backend does that from createPoolInstance (see Service::loadScript contract).
class DLL_LINKAGE ScriptHandler final : public IHandlerBase
{
public:
	ScriptHandler();
	~ScriptHandler();

	std::vector<JsonNode> loadLegacyData() override;

	void loadObject(std::string scope, std::string name, const JsonNode & data) override;
	void loadObject(std::string scope, std::string name, const JsonNode & data, size_t index) override;

	void afterLoadFinalization() override;

	/// Returns every script that hooks the given event, in load order. Used by engine hook sites,
	/// which then fetch each script's context from the session pool and call callGlobal(...).
	std::vector<const scripting::Script *> getScriptsFor(ScriptEventKind kind) const;

private:
	/// One loaded general script plus its declared bindings.
	struct ScriptEntry
	{
		std::string scriptId;                       ///< scope + ':' + name
		std::string source;                         ///< VFS source path (extension included)
		std::shared_ptr<scripting::Script> script;  ///< owned for game lifetime
		std::vector<ScriptEventKind> events;        ///< parsed from "implements"
	};

	std::vector<ScriptEntry> scripts;                                     ///< owns all scripts
	std::multimap<ScriptEventKind, const scripting::Script *> byEvent;    ///< dispatch index

	static std::optional<ScriptEventKind> parseEventKind(const std::string & implements);
};
