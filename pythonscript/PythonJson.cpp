/*
 * PythonJson.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "PythonJson.h"

#include "../lib/json/JsonNode.h"

namespace py = pybind11;

namespace scripting::json
{

py::object toPython(const JsonNode & node)
{
	switch(node.getType())
	{
	case JsonNode::JsonType::DATA_BOOL:
		return py::bool_(node.Bool());
	case JsonNode::JsonType::DATA_FLOAT:
		return py::float_(node.Float());
	case JsonNode::JsonType::DATA_INTEGER:
		return py::int_(node.Integer());
	case JsonNode::JsonType::DATA_STRING:
		return py::str(node.String());
	case JsonNode::JsonType::DATA_VECTOR:
	{
		py::list result;
		for(const auto & entry : node.Vector())
			result.append(toPython(entry));
		return result;
	}
	case JsonNode::JsonType::DATA_STRUCT:
	{
		py::dict result;
		for(const auto & [key, value] : node.Struct())
			result[py::str(key)] = toPython(value);
		return result;
	}
	default:
		return py::none();
	}
}

JsonNode toJson(const py::handle & value)
{
	JsonNode result;

	if(value.is_none())
		return result;

	// bool must be tested before int: a Python bool is an int subtype
	if(py::isinstance<py::bool_>(value))
	{
		result.Bool() = value.cast<bool>();
		return result;
	}
	if(py::isinstance<py::int_>(value))
	{
		result.Integer() = value.cast<si64>();
		return result;
	}
	if(py::isinstance<py::float_>(value))
	{
		result.Float() = value.cast<double>();
		return result;
	}
	if(py::isinstance<py::str>(value))
	{
		result.String() = value.cast<std::string>();
		return result;
	}
	if(py::isinstance<py::list>(value) || py::isinstance<py::tuple>(value))
	{
		result.Vector(); // coerce to vector type even when empty
		for(const auto & entry : value.cast<py::sequence>())
			result.Vector().push_back(toJson(entry));
		return result;
	}
	if(py::isinstance<py::dict>(value))
	{
		result.Struct(); // coerce to struct type even when empty
		for(const auto & [key, entry] : value.cast<py::dict>())
		{
			if(!py::isinstance<py::str>(key))
			{
				logMod->warn("Python->JSON: dropping non-string dict key '%s'", std::string(py::str(key)));
				continue;
			}
			result.Struct()[key.cast<std::string>()] = toJson(entry);
		}
		return result;
	}

	logMod->warn("Python->JSON: unconvertible value of type '%s' converted to null",
		std::string(py::str(value.get_type())));
	return result;
}

}
