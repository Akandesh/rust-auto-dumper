#pragma once
#include <fstream>
#include <json.hpp>
#include "xxx.h"

class header_encrypted
{
	void dheader();
	void timestamp();
	std::ofstream stream;
	bool inlinex = false;
public:
	void dump(std::vector<offset_parent_t>& data);
	header_encrypted(bool inlinex) { this->inlinex = inlinex; }
};
