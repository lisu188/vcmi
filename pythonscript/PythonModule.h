/*
 * PythonModule.h, part of VCMI engine
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

class PythonScriptInstance;

/// Top-level Python scripting service; mirrors LuaModule. Registered as a second backend in the
/// composite ScriptingHandler when the engine is built with ENABLE_PYTHON.
///
/// v1 scope: general (non-spell-effect) scripts only - loadScript for ".py" sources plus the
/// Context::callGlobal hook path. installScripting intentionally registers no spell-effect
/// factory yet; when the pybind11 game-API binding tree lands, the "python" factory plugs in
/// there exactly like Lua's "lua" factory.
class DLL_LINKAGE PythonModule final : public Service
{
public:
	PythonModule();
	~PythonModule();

	void installScripting(spells::effects::SpellEffectService * spellEffects) override;

	std::unique_ptr<Pool> createPoolInstance(const Environment * ENV) const override;

	std::shared_ptr<Script> loadScript(const std::string & scope, const std::string & source) override;

	void exportDocs(const boost::filesystem::path & outDir) const override;

	/// Initializes the process-wide host CPython interpreter exactly once, hardened for
	/// determinism: isolated config (env vars and user site ignored), fixed hash seed
	/// (PYTHONHASHSEED=0 equivalent - process-global, shared by all sub-interpreters), no signal
	/// handlers. The main GIL is released after init so any engine thread can create and drive
	/// sub-interpreters. Never finalized: tearing CPython down from static-destructor context is
	/// crash-prone and the OS reclaims everything at process exit.
	/// Returns false (and logs) if initialization failed; all contexts then stay inert.
	static bool ensureHostInterpreter();

private:
	using ScriptPtr = std::shared_ptr<PythonScriptInstance>;
	using ScriptMap = std::map<std::string, ScriptPtr>;

	/// General scripts loaded via loadScript, keyed by scope + ':' + source. Owner-of-record;
	/// these outlive every pool and are re-registered into each pool by createPoolInstance.
	ScriptMap generalScripts;
};
}
