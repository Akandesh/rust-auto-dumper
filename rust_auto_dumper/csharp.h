#pragma once
#include <fstream>
#include <json.hpp>
#include "xxx.h"

class csharp
{
	void header();
	void timestamp();
	std::ofstream stream;
public:
	void dump(std::vector<offset_parent_t>& data, std::unordered_map<std::string, std::string>& scriptoff);
	csharp() { }
};