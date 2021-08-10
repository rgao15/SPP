// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPEngine.h"

namespace SPP
{
	SPP_ENGINE_API std::string GAssetPath;

	AssetPath::AssetPath(const char* InRelPath) : _relPath(InRelPath)
	{
		_finalPath = GAssetPath + InRelPath;
	}

	const char* AssetPath::operator *() const
	{
		return _finalPath.c_str();
	}

	std::string AssetPath::GetExtension() const
	{
		return std::filesystem::path(_finalPath).extension().generic_string();
	}

	std::string AssetPath::GetRelativePath() const
	{
		return _relPath;
	}
}
