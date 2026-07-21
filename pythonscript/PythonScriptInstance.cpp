/*
 * PythonScriptInstance.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "PythonContext.h"
#include "PythonScriptInstance.h"

#include "../lib/filesystem/Filesystem.h"

namespace scripting
{

PythonScriptInstance::PythonScriptInstance(PythonModule & host,
	const std::string & baseScope, const std::string & basePath,
	const std::vector<std::pair<std::string, std::string>> & patches)
	: host(host)
	, baseModScope(baseScope)
	, baseSourcePath(basePath)
{
	loadLayer(baseScope, basePath);
	for(const auto & [scope, path] : patches)
		loadLayer(scope, path);
}

PythonScriptInstance::~PythonScriptInstance() = default;

void PythonScriptInstance::loadLayer(const std::string & modScope, const std::string & sourcePath)
{
	ScriptPath sourcePathId = ScriptPath::builtinTODO(sourcePath).addPrefix("SCRIPTS/");

	auto * loader = CResourceHandler::get(modScope);
	if(!loader->existsResource(sourcePathId))
	{
		logMod->error("Script layer not found: %s:%s", modScope, sourcePath);
		return;
	}

	Layer layer;
	auto rawData = loader->load(sourcePathId)->readAll();
	layer.sourceText = std::string(reinterpret_cast<char *>(rawData.first.get()), rawData.second);
	layer.identifier = modScope + ':' + sourcePath;
	layers.push_back(std::move(layer));
}

std::shared_ptr<PythonContext> PythonScriptInstance::createContext(const Environment * ENV) const
{
	return std::make_shared<PythonContext>(this, ENV);
}

}
