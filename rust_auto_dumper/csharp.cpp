#include <chrono>
#include "csharp.h"


void csharp::dump(std::vector<offset_parent_t>& data, std::unordered_map<std::string, std::string>& scriptoff)
{
#ifndef STAGING
	stream = std::ofstream("dump\\rust.cs");
#else
	stream = std::ofstream("dump_staging\\rust.cs");
#endif
	this->header();
	stream << "namespace blazedumper {" << std::endl;
	this->timestamp();

	for (auto& i : scriptoff) {
		std::string n = i.first;
		std::replace(n.begin(), n.end(), '.', '_');
		stream << "    public static class " << n << std::endl << "    {" << std::endl;
		stream << "    public const Int32 " << "offset" << " = 0x" << std::hex << std::uppercase << std::atoi(i.second.c_str()) << ";" << std::endl;
		stream << "    }" << std::endl;
	}
	for (auto& item : data) {
		stream << "    public static class " << item.name << std::endl <<  "    {" << std::endl;
		for (auto& of : item.offsets) {
			stream << "        public const Int32 " << of.name << " = 0x" << std::hex << std::uppercase << of.offset << "; // " << of.type << std::endl;
		}
		stream << "    }" << std::endl;
	}
	stream << "} // namespace blazedumper" << std::endl;
	stream.close();
}

void csharp::header()
{
	stream << "using System;" << std::endl;
}

void csharp::timestamp()
{
	//stream << "    public const Int32 timestamp = " <<
	//	std::chrono::system_clock::now( ).time_since_epoch( ).count( ) << std::endl; // hours since epoch
}
