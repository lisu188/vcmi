/*
 * PythonModule.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "PythonModule.h"

#include "PythonScriptInstance.h"
#include "PythonScriptPool.h"

#include <fstream>

namespace scripting
{

PythonModule::PythonModule() = default;
PythonModule::~PythonModule() = default;

void PythonModule::installScripting(spells::effects::SpellEffectService * spellEffects)
{
	// v1: no spell-effect factory yet. Once the pybind11 game-API bindings land, register the
	// "python" factory here, mirroring LuaModule's registerFactory("lua", ...).
}

std::unique_ptr<Pool> PythonModule::createPoolInstance(const Environment * ENV) const
{
	auto result = std::make_unique<PythonScriptPool>(*this, ENV);
	for(const auto & [id, script] : generalScripts)
		result->registerScript(script.get());
	return result;
}

std::shared_ptr<Script> PythonModule::loadScript(const std::string & scope, const std::string & source)
{
	// Extension dispatch: this backend only handles .py sources; anything else is another
	// backend's job (composite ScriptingHandler will try the next backend on nullptr).
	if(source.size() < 3 || source.compare(source.size() - 3, 3, ".py") != 0)
		return nullptr;

	std::string id = scope + ':' + source;

	auto it = generalScripts.find(id);
	if(it != generalScripts.end())
		return it->second; // already loaded (declared twice) - share the same instance

	if(!ensureHostInterpreter())
		return nullptr;

	// General scripts have no patch layers; the ctor reads the base source from VFS SCRIPTS/.
	auto script = std::make_shared<PythonScriptInstance>(*this, scope, source,
		std::vector<std::pair<std::string, std::string>>{});

	if(script->layers.empty())
	{
		logMod->error("Python backend failed to load script '%s'", id);
		return nullptr;
	}

	generalScripts[id] = script;
	return script;
}

void PythonModule::exportDocs(const boost::filesystem::path & outDir) const
{
	std::ofstream out((outDir / "Python_API.md").string());
	out << "# VCMI Python scripting API (v1)\n\n"
	    << "General engine-event scripts only. A script is a plain module; engine hooks are\n"
	    << "module-level functions called with a single dict of identifiers and returning plain data.\n\n"
	    << "Available imports: `math`, `vcmi`.\n\n"
	    << "## Module `vcmi`\n\n"
	    << "| Function | Description |\n"
	    << "| -------- | ----------- |\n"
	    << "| `logInfo(message)` | Log at INFO level (also the target of `print`) |\n"
	    << "| `logWarn(message)` | Log at WARN level |\n"
	    << "| `logError(message)` | Log at ERROR level |\n\n"
	    << "See docs/developers/Python_Scripting_System.md for the sandbox and determinism rules.\n";
}

bool PythonModule::ensureHostInterpreter()
{
	static const bool initialized = []() -> bool
	{
		PyConfig config;
		PyConfig_InitIsolatedConfig(&config);
		config.use_hash_seed = 1;
		config.hash_seed = 0; // deterministic str/bytes hashing across runs and machines
		config.site_import = 0;
		config.install_signal_handlers = 0;

		PyStatus status = Py_InitializeFromConfig(&config);
		PyConfig_Clear(&config);

		if(PyStatus_Exception(status))
		{
			logGlobal->error("Failed to initialize Python host interpreter: %s",
				status.err_msg ? status.err_msg : "unknown error");
			return false;
		}

		// Release the main interpreter's GIL: sub-interpreters are created and driven from
		// whichever engine thread owns the script pool, never from here.
		PyEval_SaveThread();
		return true;
	}();

	return initialized;
}

}
