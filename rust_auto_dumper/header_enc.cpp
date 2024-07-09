#include <chrono>
#include "header_enc.h"

void header_encrypted::dump(std::vector<offset_parent_t>& data)
{
	if (inlinex)
#ifndef STAGING
		stream = std::ofstream("dump\\encrypted_inline.h");
#else
		stream = std::ofstream("dump_staging\\encrypted_inline.h");
#endif
	else
#ifndef STAGING
		stream = std::ofstream("dump\\encrypted.h");
#else
		stream = std::ofstream("dump_staging\\encrypted.h");
#endif
	this->dheader();
	stream << "namespace blazedumper {" << std::endl;
	this->timestamp();


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

void header_encrypted::dheader()
{
	stream << "#pragma once" << std::endl;
	stream << "#include <cstdint>" << std::endl << std::endl;
}

void header_encrypted::timestamp()
{
	//stream << "constexpr ::std::int64_t timestamp = " << 
	//	std::chrono::system_clock::now( ).time_since_epoch( ).count( ) << std::endl; // hours since epoch
}



