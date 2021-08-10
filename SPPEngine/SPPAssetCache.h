// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPEngine.h"
#include "SPPSerialization.h"
#include <filesystem>

namespace SPP
{
	SPP_ENGINE_API std::shared_ptr<BinaryBlobSerializer> GetCachedAsset(const AssetPath& AssetPath, const std::string Tag = "");
	SPP_ENGINE_API void PutCachedAsset(const AssetPath& AssetPath, const BinaryBlobSerializer& InBinaryBlob, const std::string Tag = "");
}