// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPMesh.h"

namespace SPP
{
	SPP_GRAPHICS_API bool LoadUsingAssImp(const AssetPath& FileName, LoadedMeshes& oMeshes);
}