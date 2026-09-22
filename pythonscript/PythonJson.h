/*
 * PythonJson.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <pybind11/pybind11.h>

class JsonNode;

namespace scripting
{

/// Conversion between the engine's JsonNode and Python objects. This is the only data channel
/// between the engine and Python scripts on the neutral Context::callGlobal path:
/// null <-> None, bool <-> bool, integer <-> int, float <-> float, string <-> str,
/// vector <-> list, struct <-> dict (string keys).
///
/// Conversions must run with the target interpreter active (GIL held by the caller).
namespace json
{

pybind11::object toPython(const JsonNode & node);

/// Unconvertible Python values (arbitrary objects, non-string dict keys, ...) convert to a
/// null JsonNode with a logged warning - scripts must return plain data on this path.
JsonNode toJson(const pybind11::handle & value);

}
}
