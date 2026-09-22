/*
 * PythonContext.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/scripting/Service.h>

#include <pybind11/embed.h>
#include <pybind11/subinterpreter.h>

class JsonNode;

namespace scripting
{

class PythonScriptInstance;

/// Manages one isolated CPython sub-interpreter for one script - the analogue of LuaContext's
/// one-lua_State-per-script model. Does not survive map restarts; destroyed and recreated with
/// the owning PythonScriptPool.
///
/// Isolation & sandbox (see docs/developers/Python_Scripting_System.md for the threat model):
/// - own-GIL sub-interpreter (pybind11 >= 3.0 / Python >= 3.12): separate builtins, sys.modules
///   and import state per script; scripts cannot observe each other's globals.
/// - scripts execute with a curated __builtins__ whitelist (no open/exec/eval/compile/input/
///   getattr/setattr/id/...), an import allowlist enforced via a sys.meta_path guard
///   (math + the embedded vcmi module only), audit-hook tripwires, and print redirected to
///   the VCMI logger.
/// - determinism: the host interpreter is initialized with a fixed hash seed (PythonModule),
///   and nondeterministic modules (random, time, ...) are not importable.
///
/// Script convention: layers execute sequentially in one module namespace; engine hooks are
/// module-level functions looked up by name, called with a single dict parameter and returning
/// plain data (converted via scripting::json).
class PythonContext final : public Context
{
public:
	PythonContext(const PythonScriptInstance * script, const Environment * ENV);
	~PythonContext();

	/// Creates the sub-interpreter, installs the sandbox and executes all script layers.
	/// On failure the context stays inert: callGlobal returns null for every call.
	void initialize();

	JsonNode callGlobal(const std::string & functionName, const JsonNode & parameters) override;

private:
	std::mutex mutex;

	const PythonScriptInstance * script;
	const Environment * env;

	std::optional<pybind11::subinterpreter> interpreter;

	/// The script's module namespace. Holds interpreter-owned references: must only be touched
	/// (and released) while this context's sub-interpreter is active.
	pybind11::object scriptGlobals;

	bool initialized = false;
};

}
