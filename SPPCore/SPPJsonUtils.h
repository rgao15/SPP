// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "json/json.h"
#include <string>

namespace SPP
{
	SPP_CORE_API bool StringToJson(const std::string& InString, Json::Value& outValue);
	SPP_CORE_API bool FileToJson(const char* FileName, Json::Value& outValue);
}