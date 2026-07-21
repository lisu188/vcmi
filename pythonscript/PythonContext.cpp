/*
 * PythonContext.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "PythonContext.h"

#include "PythonJson.h"
#include "PythonModule.h"
#include "PythonScriptInstance.h"

#include "../lib/json/JsonNode.h"

namespace py = pybind11;

namespace scripting
{

/// Executed inside every fresh sub-interpreter BEFORE any script layer. Runs with the real
/// builtins (the bootstrap itself needs them), installs the sandbox, and leaves the curated
/// builtins dict in its namespace as __vcmi_restricted_builtins__ for the host to pick up.
///
/// Layer summary (defense-in-depth, not an adversarial boundary - see the developer docs):
///  1. sys.meta_path import allowlist - only 'math' and the embedded 'vcmi' module resolve.
///  2. Curated __builtins__ whitelist for script code; print -> vcmi.logInfo.
///  3. Audit-hook tripwires for file/process/socket access attempted below the Python level.
///  4. Dangerous preloaded modules dropped from this interpreter's sys.modules.
static const char * const SANDBOX_BOOTSTRAP = R"PYVCMI(
import sys as _sys
import builtins as _builtins
import vcmi as _vcmi

_ALLOWED_MODULES = frozenset({'math', 'vcmi'})

class _VcmiImportGuard:
    def find_spec(self, fullname, path=None, target=None):
        if fullname.split('.')[0] in _ALLOWED_MODULES:
            return None  # defer to the regular finders
        raise ImportError("import of '" + fullname + "' is blocked by the VCMI sandbox")

_sys.meta_path.insert(0, _VcmiImportGuard())

def _vcmi_audit(event, args):
    _blocked = ('open', 'os.', 'socket.', 'subprocess.', 'shutil.', 'ctypes.', 'webbrowser.')
    for _prefix in _blocked:
        if event == _prefix or event.startswith(_prefix):
            raise RuntimeError('blocked by the VCMI sandbox: ' + event)

_sys.addaudithook(_vcmi_audit)

for _name in ('os', 'subprocess', 'socket', 'shutil', 'pathlib', 'tempfile', 'ctypes',
              'threading', 'multiprocessing', 'webbrowser', 'random', 'secrets', 'time'):
    _sys.modules.pop(_name, None)

def _blocked_builtin(name):
    def _fail(*args, **kwargs):
        raise RuntimeError("'" + name + "' is disabled in the VCMI sandbox")
    return _fail

def _vcmi_print(*args, **kwargs):
    _vcmi.logInfo(' '.join(str(a) for a in args))

_SAFE_NAMES = (
    'abs', 'all', 'any', 'bool', 'bytes', 'callable', 'chr', 'dict', 'divmod', 'enumerate',
    'filter', 'float', 'format', 'frozenset', 'hasattr', 'hash', 'hex', 'int', 'isinstance',
    'issubclass', 'iter', 'len', 'list', 'map', 'max', 'min', 'next', 'object', 'oct', 'ord',
    'pow', 'range', 'repr', 'reversed', 'round', 'set', 'slice', 'sorted', 'str', 'sum',
    'tuple', 'zip', 'True', 'False', 'None', 'Exception', 'ValueError', 'TypeError',
    'KeyError', 'IndexError', 'RuntimeError', 'StopIteration', 'ArithmeticError',
    'ZeroDivisionError', 'AttributeError', 'LookupError', 'NotImplementedError',
)

_restricted = {}
for _name in _SAFE_NAMES:
    if hasattr(_builtins, _name):
        _restricted[_name] = getattr(_builtins, _name)

_restricted['print'] = _vcmi_print
# The import STATEMENT stays available; every import still passes _VcmiImportGuard above.
_restricted['__import__'] = _builtins.__import__

for _name in ('open', 'exec', 'eval', 'compile', 'input', 'breakpoint', 'globals', 'locals',
              'vars', 'dir', 'getattr', 'setattr', 'delattr', 'memoryview', 'type', 'id',
              'help', 'exit', 'quit'):
    _restricted[_name] = _blocked_builtin(_name)

__vcmi_restricted_builtins__ = _restricted
)PYVCMI";

PythonContext::PythonContext(const PythonScriptInstance * script, const Environment * ENV)
	: script(script)
	, env(ENV)
{
}

PythonContext::~PythonContext()
{
	// scriptGlobals belongs to the sub-interpreter: release it while that interpreter is
	// active, before the interpreter itself is destroyed by ~subinterpreter.
	if(interpreter)
	{
		py::subinterpreter_scoped_activate guard(*interpreter);
		scriptGlobals = py::object();
	}
}

void PythonContext::initialize()
{
	std::lock_guard guard(mutex);

	if(script->layers.empty())
	{
		logMod->error("Python script '%s' has no source layers, context stays inert", script->getIdentifier());
		return;
	}

	if(!PythonModule::ensureHostInterpreter())
		return;

	try
	{
		interpreter.emplace(py::subinterpreter::create());
	}
	catch(const std::exception & e)
	{
		logMod->error("Python script '%s': failed to create sub-interpreter: %s", script->getIdentifier(), e.what());
		interpreter.reset();
		return;
	}

	py::subinterpreter_scoped_activate active(*interpreter);
	// error_already_set holds references into this interpreter - it must never escape the
	// activation scope, so every Python failure is handled right here.
	try
	{
		py::dict bootstrapNs;
		py::exec(SANDBOX_BOOTSTRAP, bootstrapNs);

		py::dict globals;
		globals["__builtins__"] = bootstrapNs["__vcmi_restricted_builtins__"];
		globals["__name__"] = py::str(script->getIdentifier());

		for(const auto & layer : script->layers)
		{
			try
			{
				py::exec(layer.sourceText.c_str(), globals);
			}
			catch(const py::error_already_set & e)
			{
				// Mirror Lua layer semantics: log the failing layer, keep already-loaded ones.
				logMod->error("Script layer '%s' failed to run: %s", layer.identifier, e.what());
			}
		}

		scriptGlobals = std::move(globals);
		initialized = true;
	}
	catch(const py::error_already_set & e)
	{
		logMod->error("Python script '%s': sandbox setup failed: %s", script->getIdentifier(), e.what());
	}
	catch(const std::exception & e)
	{
		logMod->error("Python script '%s': initialization failed: %s", script->getIdentifier(), e.what());
	}
}

JsonNode PythonContext::callGlobal(const std::string & functionName, const JsonNode & parameters)
{
	std::lock_guard guard(mutex);

	if(!initialized || !interpreter)
		return JsonNode();

	py::subinterpreter_scoped_activate active(*interpreter);
	try
	{
		py::dict globals = scriptGlobals.cast<py::dict>();

		if(!globals.contains(functionName.c_str()))
		{
			logMod->error("Script '%s': function '%s' not found", script->getIdentifier(), functionName);
			return JsonNode();
		}

		py::object function = globals[functionName.c_str()];
		py::object result = function(json::toPython(parameters));
		return json::toJson(result);
	}
	catch(const py::error_already_set & e)
	{
		logMod->error("Script '%s', function '%s': %s", script->getIdentifier(), functionName, e.what());
		return JsonNode();
	}
	catch(const std::exception & e)
	{
		logMod->error("Script '%s', function '%s': %s", script->getIdentifier(), functionName, e.what());
		return JsonNode();
	}
}

}
