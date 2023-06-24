#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <map>
#include <exception>
#include <memory>
#include <functional>

typedef std::map<std::string, std::string> AnisetteData;
std::shared_ptr<AnisetteData> fetchAnisette();