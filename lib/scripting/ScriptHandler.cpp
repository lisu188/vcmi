/*
 * ScriptHandler.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "ScriptHandler.h"

#include "../GameLibrary.h"
#include "../json/JsonNode.h"

#include <vcmi/scripting/Service.h>

ScriptHandler::ScriptHandler() = default;
ScriptHandler::~ScriptHandler() = default;

std::vector<JsonNode> ScriptHandler::loadLegacyData()
{
	return {};
}

std::optional<ScriptEventKind> ScriptHandler::parseEventKind(const std::string & implements)
{
	static const std::map<std::string, ScriptEventKind> lookup = {
		{"ON_PLAYER_TURN_START", ScriptEventKind::ON_PLAYER_TURN_START},
		// {"ON_OBJECT_VISIT", ScriptEventKind::ON_OBJECT_VISIT},
		// {"ON_BATTLE_START", ScriptEventKind::ON_BATTLE_START},
	};

	auto it = lookup.find(implements);
	if(it == lookup.end())
		return std::nullopt;
	return it->second;
}

void ScriptHandler::loadObject(std::string scope, std::string name, const JsonNode & data)
{
	ScriptEntry entry;
	entry.scriptId = scope + ':' + name;
	entry.source = data["source"].String();

	// Register the identifier so scripts are addressable by "scope:script:name" like other content.
	registerObject(scope, "script", name, data, static_cast<si32>(scripts.size()));

	// Load the script through the composite scripting service. The backend that recognizes the
	// source builds and OWNS the concrete Script, retains it for pool registration, and returns it.
	// Access the non-const scriptHandler member directly: GameLibrary::scripts() is const and yields
	// a const Service*, but loadScript mutates backend state.
	std::shared_ptr<scripting::Script> loaded;
	if(LIBRARY->scriptHandler)
		loaded = LIBRARY->scriptHandler->loadScript(scope, entry.source);

	if(!loaded)
	{
		logMod->error("Script '%s': no scripting backend could load source '%s'", entry.scriptId, entry.source);
		scripts.push_back(std::move(entry));
		return;
	}
	entry.script = std::move(loaded);

	// "implements" may be a single string; a "scripts" content object with no event binding is
	// accepted but simply never dispatched (analogous to ANYTHING / a library-only script).
	const JsonNode & implementsNode = data["implements"];
	if(implementsNode.isString())
	{
		if(auto kind = parseEventKind(implementsNode.String()))
			entry.events.push_back(*kind);
		else if(implementsNode.String() != "ANYTHING" && implementsNode.String() != "BATTLE_EFFECT")
			logMod->warn("Script '%s': unknown 'implements' value '%s'", entry.scriptId, implementsNode.String());
	}

	scripts.push_back(std::move(entry));

	const auto & stored = scripts.back();
	for(auto kind : stored.events)
		byEvent.emplace(kind, stored.script.get());
}

void ScriptHandler::loadObject(std::string scope, std::string name, const JsonNode & data, size_t index)
{
	throw std::runtime_error("ScriptHandler: indexed (legacy) load is not supported");
}

void ScriptHandler::afterLoadFinalization()
{
}

std::vector<const scripting::Script *> ScriptHandler::getScriptsFor(ScriptEventKind kind) const
{
	std::vector<const scripting::Script *> result;
	auto range = byEvent.equal_range(kind);
	for(auto it = range.first; it != range.second; ++it)
		result.push_back(it->second);
	return result;
}
