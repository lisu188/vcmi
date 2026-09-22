/*
 * PythonScriptPool.h, part of VCMI engine
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

class PythonModule;
class PythonScriptInstance;

/// Owned (via CompositePool) by CGameState; holds the live PythonContext for every registered
/// Python script in the current game session. Mirrors LuaScriptPool, including the
/// nullptr-on-miss getContext contract used by the composite pool.
class PythonScriptPool final : public Pool
{
public:
	PythonScriptPool(const PythonModule & pythonModule, const Environment * ENV);

	std::shared_ptr<Context> getContext(const Script * script) const override;

	void registerScript(const PythonScriptInstance * script);

private:
	std::map<const Script *, std::shared_ptr<Context>> cache;

	const Environment * env;
};
}
