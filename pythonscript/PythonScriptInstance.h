/*
 * PythonScriptInstance.h, part of VCMI engine
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
class PythonContext;

/// Holds the source code of a Python script (optionally with patch layers stacked over a base)
/// and metadata; one instance per logical script. Mirrors LuaScriptInstance.
/// Owned by PythonModule and used as a factory to create PythonContext instances per game session.
class PythonScriptInstance final : public Script
{
public:
	struct Layer
	{
		std::string sourceText;
		std::string identifier; ///< modScope + ':' + sourcePath, used for error reporting
	};

	/// Builds the chain: layer[0] is the base, layer[1..] are patches in declared order.
	/// All layers execute sequentially in the same module namespace, so later layers may
	/// override functions defined by earlier ones (same semantics as Lua patch layers).
	/// Failed-to-load patch layers are skipped with a logged error; failed base load leaves layers empty.
	PythonScriptInstance(PythonModule & host,
		const std::string & baseScope, const std::string & basePath,
		const std::vector<std::pair<std::string, std::string>> & patches);
	~PythonScriptInstance();

	std::vector<Layer> layers;

	PythonModule & host;

	std::shared_ptr<PythonContext> createContext(const Environment * ENV) const;

	std::string getIdentifier() const override { return baseModScope + baseSourcePath; }

private:
	std::string baseModScope;
	std::string baseSourcePath;
	void loadLayer(const std::string & modScope, const std::string & sourcePath);
};
}
