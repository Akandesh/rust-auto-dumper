#include <chrono>
#include "header.h"

void header::dump(std::vector<offset_parent_t>& data, std::unordered_map<std::string, std::string>& scriptoff)
{
	if (inlinex)
#ifndef STAGING
		stream = std::ofstream("dump\\rust_inline.h");
#else
		stream = std::ofstream("dump_staging\\rust_inline.h");
#endif
	else
#ifndef STAGING
		stream = std::ofstream("dump\\rust.h");
#else
		stream = std::ofstream("dump_staging\\rust.h");
#endif
	this->dheader();
	stream << "namespace blazedumper {" << std::endl;
	this->timestamp();

	for (auto& i : scriptoff) {
		std::string n = i.first;
		std::replace(n.begin(), n.end(), '.', '_');
		stream << "    " << (inlinex ? "inline " : "") << "constexpr ::std::ptrdiff_t " << n << " = 0x" << std::hex << std::uppercase << std::atoi(i.second.c_str()) << ";" << std::endl;
	}

	for (auto& item : data) {
		stream << "    namespace " << item.name << " {" << std::endl;
		for (auto& of : item.offsets) {
			stream << "        " << (inlinex ? "inline " : "") << "constexpr ::std::ptrdiff_t " << of.name << " = 0x" << std::hex << std::uppercase << of.offset << "; // " << of.type << std::endl;
		}
		stream << "    } // namespace " << item.name << std::endl;
	}
	stream << "} // namespace blazedumper" << std::endl;
	stream.close();
}

void header::dheader()
{
	stream << "#pragma once" << std::endl;
	stream << "#include <cstdint>" << std::endl << std::endl;
}

void header::timestamp()
{
	//stream << "constexpr ::std::int64_t timestamp = " << 
	//	std::chrono::system_clock::now( ).time_since_epoch( ).count( ) << std::endl; // hours since epoch
}



