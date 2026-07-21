/*
 * VcmiModule.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../StdInc.h"

namespace py = pybind11;

/// The embedded `vcmi` module - the only engine surface visible to sandboxed Python scripts.
///
/// The per_interpreter_gil tag is mandatory: without it, importing the module inside an
/// own-GIL sub-interpreter raises ImportError. Because every sub-interpreter imports its own
/// copy, nothing here may rely on shared mutable state.
///
/// v1 surface is deliberately tiny (logging + print redirect target). The full game API
/// (GAME/LIBRARY parity with luascript/api/) is the next increment; it plugs into this module.
PYBIND11_EMBEDDED_MODULE(vcmi, m, py::multiple_interpreters::per_interpreter_gil())
{
	m.doc() = "VCMI engine interface for sandboxed mod scripts";

	m.def("logInfo", [](const std::string & message) { logScript->info("%s", message); },
		"Log a message at INFO level (also the target of print())");
	m.def("logWarn", [](const std::string & message) { logScript->warn("%s", message); },
		"Log a message at WARN level");
	m.def("logError", [](const std::string & message) { logScript->error("%s", message); },
		"Log a message at ERROR level");
}
