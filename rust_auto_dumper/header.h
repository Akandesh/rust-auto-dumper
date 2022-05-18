#pragma once
#include <fstream>
#include <json.hpp>
#include "xxx.h"

class header
{
	void dheader();
	void timestamp();
	std::ofstream stream;
	bool inlinex = false;
public:
	void dump(std::vector<offset_parent_t>& data, std::unordered_map<std::string, std::string>& scriptoff);
	header( bool inlinex) { this->inlinex = inlinex; }
};
