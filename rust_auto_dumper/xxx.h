#pragma once
#include <string>
#include <basetsd.h>


struct offset_entry_t {
	std::string type;
	std::string name;
	UINT32 offset;
};

struct offset_parent_t {
	std::string name;
	std::vector<offset_entry_t> offsets;
};