#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <chrono>
#include <thread>
#include <sstream>
#include <regex>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <format>

#include <json.hpp>
#include "xxx.h"
#include "console.hpp"
#include "header.h"
#include "header_enc.h"
#include "csharp.h"


void update_readme();
void update_check();
void dump_to_github();
std::string get_current_time();

int current_log = 0;
std::ofstream log_stream;
std::ofstream update_log_stream;

std::string exec(const char* cmd, std::ofstream* log_stream = nullptr) {
	std::array<char, 128> buffer;
	std::string result;
	std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
	if (!pipe) {
		throw std::runtime_error("popen() failed!");
	}

	std::string time = get_current_time();
	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
		std::string data = buffer.data();
		result += data;
		if (log_stream != nullptr && log_stream->good() && log_stream->is_open()) {
			*log_stream << "[" << time << "] " << data;
		}
	}

	// Do not close the log_stream here. Let the caller handle it.

	return result;
}

std::string remote_build;
std::string local_build;
bool updated = false;

bool update() {
	console::set_title_message("Updating..");
	try {
		console::print_success(true, "Update available");
		console::print(normal, "Local build: ", false);
		console::heck(failure, local_build.c_str());
		console::print(normal, "Remote build: ", false);
		console::heck(success, remote_build.c_str());
#ifndef STAGING
		std::filesystem::path f{ std::string("logs/live_log") + std::to_string(current_log) + std::string(".log") };
		log_stream = std::ofstream(f, std::ofstream::trunc);
		current_log++;
		if (current_log >= 20) {
			current_log = 0;
		}
		auto response = exec("update_rust.bat", &log_stream);
		log_stream.close();
#else
		std::filesystem::path f{ std::string("logs/staging_log") + std::to_string(current_log) + std::string(".log") };
		log_stream = std::ofstream(f, std::ofstream::trunc);
		current_log++;
		if (current_log >= 20) {
			current_log = 0;
		}
		auto response = exec("update_rust-staging.bat", &log_stream);
#endif
		if (response.find("Success!") != std::string::npos) {
			updated = true;
			local_build = remote_build;

#ifndef STAGING
			std::ofstream outstream("buildid.txt");
#else
			std::ofstream outstream("buildid-staging.txt");
#endif
			outstream << local_build;
			outstream.close();
		}
		else {
			updated = false;
			std::this_thread::sleep_for(std::chrono::seconds(30));
		}
		if (updated) {
			dump_to_github();
		}
		return updated;
	}
	catch (std::exception& e) {
		console::set_title_message("Update failed..");
		console::print_failure(true, "Update failed: %s", e.what());
		return false;
	}
}

void update_check() {
	console::set_title_message("Checking for update..");
	try {
		static std::regex regexp(R"(^\s\s\s\"public\"\s+\{\s+\"buildid\"\s+\"(\d+)\")");
#ifndef STAGING
		std::filesystem::path f{ "logs/update_check.log" };
		if (exists(f)) {
			std::filesystem::copy(f, "logs/update_check2.log", std::filesystem::copy_options::overwrite_existing);
		}
		update_log_stream = std::ofstream(f, std::ofstream::trunc);
		auto output = exec("checkver.bat", &update_log_stream);
#else
		std::filesystem::path f{ "logs/update_check_staging.log" };
		if (exists(f)) {
			std::filesystem::copy(f, "logs/update_check_staging2.log", std::filesystem::copy_options::overwrite_existing);
		}
		update_log_stream = std::ofstream(f, std::ofstream::trunc);
		auto output = exec("checkver-staging.bat");
#endif

		std::smatch match;
		const bool found_match = std::regex_search(output, match, regexp);
		if (!found_match) {
			console::print_failure(true, "Update match failed: %s", output.c_str());
			return;
		}

		remote_build = match[1].str();

		if (local_build != remote_build) {
			console::print_failure(true, "Local build mismatch l: %s r: %s", local_build.c_str(), remote_build.c_str());
			updated = update();
			if (updated) {
				local_build = remote_build;
				console::print_success(true, "Update was a success");
			}
		}
		else {
			updated = true;
			console::set_title_message("Checked for update, latest build: %s", local_build.c_str());
		}

	}
	catch (std::exception& e) {
		console::print_failure(true, "Exception: %s", e.what());
	}
}


int main() {
	
#ifdef _DEBUG
	update_readme();
	return 0;
#endif
#ifndef STAGING
	std::filesystem::path f{ "buildid.txt" };
#else
	std::filesystem::path f{ "buildid-staging.txt" };
#endif
	if (exists(f)) {
		std::ifstream filein(f);
		filein >> local_build;
		filein.close();
	}
	if (!local_build.empty())
		console::print(normal, "Local build is: " + local_build, true);
	while (true) {
		update_check();
		if (!updated) {
			updated = update();
			if (!updated) {
				std::this_thread::sleep_for(std::chrono::minutes(1));
				continue;
			}
			local_build = remote_build;
			continue;
		}
		std::this_thread::sleep_for(std::chrono::minutes(3));
	}
	return 0;
}

void dump_to_github() {
	console::set_title_message("Dumping..");
#ifndef STAGING
	auto output = exec("dump.bat");
#else
	auto output = exec("dump-staging.bat");
#endif
	if (output.find("Done!") != std::string::npos) {
		update_readme();
		console::set_title_message("Sending to github..");
#ifndef STAGING
		exec("script.sh");
#else
		exec("script-staging.sh");
#endif
		console::set_title_message("All done!");
	}
	else {
		console::print_failure(true, "Dumping failed! %s", output.c_str());
	}
}

std::string slurp(std::ifstream& in) {
	std::ostringstream sstr;
	sstr << in.rdbuf();
	return sstr.str();
}

std::string get_current_time()
{
	auto t = std::time(nullptr);
	auto tm = *std::localtime(&t);
	std::ostringstream oss;
	oss << std::put_time(&tm, "%d-%m-%Y %H:%M:%S");
	return oss.str();
}

std::vector<std::string> splitbyline(std::string& sentence) {
	std::stringstream ss(sentence);
	std::string to;
	std::vector<std::string> lines;
	int i = 0;

	while (std::getline(ss, to, '\n')) {
		if (i != 0 && to.size() > 5)
			lines.push_back(to);
		i++;
	}
	return lines;
}

void basic_scan(std::string classname, std::string& dumpData, offset_parent_t& out, bool static_members = false) {
	static std::regex regex1(R"((public|private|protected|internal)\s([%A-Za-z0-9]+\.?[%A-Za-z0-9\*]*?)\s([%A-Za-z0-9]+);\s\/\/\s(0x[0-9a-fA-F]+)?)"); // works on 90%
	static std::regex regex2(R"((public|private|protected|internal)\s([%A-Za-z0-9+]+\s?<[%A-Za-z0-9]+>)\s?(.*)?;\s\/\/\s(0x[0-9a-fA-F]+))"); // works on stuff like Vector<int> and thing.thing<thing>
	static std::regex regexList(R"((public|private|protected|internal)\s(List<.*?>)\s([%A-Za-z0-9]+);\s\/\/\s(0x[0-9a-fA-F]+))"); // only works on real List, append list to name
	static std::regex regexArray(R"((public|private|protected|internal)\s([%A-Za-z0-9]+\[\])\s(.*?);\s\/\/\s(0x[0-9a-fA-F]+))"); // only works on arrays
	// static std::regex regexEntityRef(R"((public|private|protected|internal)\s(EntityRef<\w+>)\s(\w+);\s\/\/\s(0x[0-9a-fA-F]+)?)"); // only works on entityrefs // REMOVED because hashed now
	static std::regex regexStatic(R"((public|private|protected|internal)\sstatic\s([%A-Za-z0-9\.]*)\s([%A-Za-z0-9]+);\s\/\/\s(0x[0-9a-fA-F]+)?)");
	static std::regex regexReadonly(R"((public|private|protected|internal)\sreadonly\s([%A-Za-z0-9]+\.?[%A-Za-z0-9]*?)\s([%A-Za-z0-9]+);\s\/\/\s(0x[0-9a-fA-F]+)?)");

	// 1. go to start of class
	// 2. shorten data by finding the next class then substr that index
	auto idx = dumpData.find(classname);
	assert(idx != std::string::npos);

	auto nextidx = dumpData.find("// Namespace:", idx + 5);
	assert(nextidx != std::string::npos);

	std::string clazz = dumpData.substr(idx, nextidx - idx);

	for (auto& line : splitbyline(clazz)) {
		std::smatch results;
		unsigned long offset_num = 0;
		if (std::regex_search(line, results, regex1)) {
			/* need to convert offset to number */
			offset_num = std::stoul(results[4].str(), nullptr, 16);
			out.offsets.emplace_back(offset_entry_t{ results[2].str(), results[3].str(), offset_num });
		}
		else if (std::regex_search(line, results, regexReadonly)) {
			/* need to convert offset to number */
			offset_num = std::stoul(results[4].str(), nullptr, 16);
			out.offsets.emplace_back(offset_entry_t{ results[2].str(), results[3].str(), offset_num });
		}
		else if (std::regex_search(line, results, regex2)) {
			/* need to convert offset to number */
			offset_num = std::stoul(results[4].str(), nullptr, 16);
			out.offsets.emplace_back(offset_entry_t{ results[2].str(), results[3].str(), offset_num });
		}
		else if (std::regex_search(line, results, regexList)) {
			/* need to convert offset to number */
			offset_num = std::stoul(results[4].str(), nullptr, 16);
			bool add = results[2].str().find("List") == std::string::npos;
			out.offsets.emplace_back(offset_entry_t{ results[2].str(), results[3].str() + (add ? "List" : ""), offset_num });
		}
		else if (std::regex_search(line, results, regexArray)) {
			/* need to convert offset to number */
			offset_num = std::stoul(results[4].str(), nullptr, 16);
			out.offsets.emplace_back(offset_entry_t{ results[2].str(), results[3].str(), offset_num });
		}
		else if (static_members && std::regex_search(line, results, regexStatic)) {
			/* need to convert offset to number */
			offset_num = std::stoul(results[4].str(), nullptr, 16);
			out.offsets.emplace_back(offset_entry_t{ results[2].str(), std::string("static_") + results[3].str(), offset_num });
		}

	}

	if (out.offsets.empty())
		DebugBreak();

}

void ReplaceStringInPlace(std::string& subject, const std::string& search,
	const std::string& replace) {
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos) {
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
}
using namespace nlohmann;
offset_parent_t add_class_dump(std::string name, std::string dump_string, std::string& dump_data, json& json_out, bool static_members) {
	ReplaceStringInPlace(name, "%", "_");
	offset_parent_t offsets = { name };
	basic_scan(dump_string, dump_data, offsets, static_members);

	json j;
	for (auto& k : offsets.offsets) {
		ReplaceStringInPlace(k.name, "%", "_");
		j[k.name] = k.offset;
	}

	json_out[name] = j;
	return offsets;
}


void update_readme() {
	console::set_title_message("Parsing dump..");
	std::unordered_map<std::string, std::string> script_offsets = {
		{"BaseEntity_TypeInfo", ""},
		{"MainCamera_TypeInfo", ""},
		{"Facepunch.Input_TypeInfo", ""},
		{"System.Collections.Generic.List<BaseGameMode>_TypeInfo", ""},
		{"BaseGameMode_TypeInfo", ""},
		{"TOD_Sky_TypeInfo", ""},
		{"ConVar.Admin_TypeInfo", ""},
		{"BasePlayer_TypeInfo", ""},
		{"ConVar.Graphics_TypeInfo", ""},
		{"OcclusionCulling_TypeInfo", ""},
		{"BaseNetworkable_TypeInfo", ""},
		{"ConsoleSystem.Index_TypeInfo", ""}
	}; // default values used for script.json


	std::vector<class_dump> classes = {
		{"BasePlayer", "public class BasePlayer : BaseCombatEntity, LootPanel"},
		{"BaseEntity", "public class BaseEntity : BaseNetworkable"},
		{"BaseCombatEntity", "public class BaseCombatEntity : BaseEntity"},
		{"BuildingPrivlidge", "public class BuildingPrivlidge : StorageContainer"},
		{"BaseProjectile", "public class BaseProjectile : AttackEntity"},
		{"Magazine", "public class BaseProjectile.Magazine"},
		{"PlayerInventory", "public class PlayerInventory : EntityComponent<BasePlayer>"},
		{"PlayerModel", "public class PlayerModel : ListComponent<PlayerModel>"},
		{"ModelState", "public class ModelState : IDisposable, Pool.IPooled, IProto"},
		{"Model", "public class Model : MonoBehaviour, IPrefabPreProcess"},
		{"RecoilProperties", "public class RecoilProperties : ScriptableObject"},
		{"BaseFishingRod", "public class BaseFishingRod : HeldEntity"},
		{"FishingBobber", "public class FishingBobber : BaseCombatEntity"},
		{"OcclusionCulling", "public class OcclusionCulling : MonoBehaviour", true},
		{"OcclusionCulling.DebugSettings", "public class OcclusionCulling.DebugSettings"},
		{"PlayerInput", "public class PlayerInput : EntityComponent<BasePlayer>"},
		{"ItemDefinition", "public class ItemDefinition : MonoBehaviour"},
		{"PlayerEyes", "public class PlayerEyes : EntityComponent<BasePlayer>"},
		{"Projectile", "public class Projectile : BaseMonoBehaviour"},
		{"ItemModProjectile", "public class ItemModProjectile : MonoBehaviour"}
	};

	// fetching values we're interested in
	if (true) {
#ifndef STAGING
		std::ifstream script_stream("dump\\script.json");
#else
		std::ifstream script_stream("dump_staging\\script.json");
#endif
		try {
			json j = json::parse(script_stream);
			auto ScriptMetadata = j.at("ScriptMetadata");
			for (auto it = ScriptMetadata.begin(); it != ScriptMetadata.end(); it++) {
				auto s = it->is_structured();
				if (!it->is_object())
					continue;
				auto obj = it.value();
				auto name = obj.at("Name").get<std::string>();

				for (auto& of : script_offsets) {
					if (of.second.size()) // if it already has data
						continue;
					// replace strings of unicode characters with the actual character i.e \u003C with <
					ReplaceStringInPlace(name, "\\u003C", "<");
					ReplaceStringInPlace(name, "\\u003E", ">");

					if (of.first == name) {
						of.second = std::to_string(obj.at("Address").get<int>());
					}
				}
			}
		}
		catch (std::exception& e) {
			console::print_failure(true, (std::string("JSON FAIL: ") + e.what()).c_str());
		}
	}

	// dump.cs
#ifndef STAGING
	std::ifstream dump("dump\\dump.cs");
#else
	std::ifstream dump("dump_staging\\dump.cs");
#endif
	std::string dumpData = slurp(dump);

	// getting rid of excess data
	auto t = dumpData.find("public class BaseCombatEntity : BaseEntity");
	if (t == std::string::npos) {
		console::print_failure(true, "Failed to find excess splitter");
		assert(t != std::string::npos);
		return;
	}
#ifdef _DEBUG


	std::string first_class = "";
	for (auto& klass : classes) {
		static size_t lowest_idx = INT_MAX;
		size_t index = dumpData.find(klass.scan);
		if (index < lowest_idx) {
			lowest_idx = index;
			first_class = klass.scan;
		}
	}

	printf("Best class is %s\n", first_class.c_str());
#endif
	dumpData = dumpData.substr(t - 30);

	json final_j;
	std::unordered_map<std::string, std::string> fixed_script_offsets = { };
	for (auto& x : script_offsets) {
		if (!x.second.size()) {
			printf("%s missing\n", x.first.c_str());
			continue;
		}
		assert(x.second.size());
		std::string n = x.first;
		std::replace(n.begin(), n.end(), '.', '_'); // replacing . with _
		std::replace(n.begin(), n.end(), '<', '_'); // replacing < with _
		n.erase(std::remove(n.begin(), n.end(), '>')); // removing >
		fixed_script_offsets.insert({ n, x.second });
		final_j[n] = std::atoi(x.second.c_str());
	}

	std::vector<offset_parent_t> final_offsets;
	for (auto& klass : classes) {
		final_offsets.emplace_back(add_class_dump(klass.name, klass.scan, dumpData, final_j, klass.static_members));
	}


	std::vector<std::string> encrypted_classes;
	size_t last_pos = 0;
	while ((last_pos = dumpData.find("public struct %", last_pos)) != std::string::npos) {
		// Find the end of the line
		size_t end_pos = dumpData.find('\n', last_pos);
		if (end_pos == std::string::npos) {
			end_pos = dumpData.length();
		}

		// Extract the full line
		std::string line = dumpData.substr(last_pos, end_pos - last_pos);
		encrypted_classes.push_back(line);

		// Move to the next position
		last_pos = end_pos + 1;
	}

	while ((last_pos = dumpData.find("private sealed class %", last_pos)) != std::string::npos) {
		// Find the end of the line
		size_t end_pos = dumpData.find('\n', last_pos);
		if (end_pos == std::string::npos) {
			end_pos = dumpData.length();
		}

		// Extract the full line
		std::string line = dumpData.substr(last_pos, end_pos - last_pos);
		encrypted_classes.push_back(line);

		// Move to the next position
		last_pos = end_pos + 1;
	}

	std::vector<offset_parent_t> encrypted_offsets;
	std::smatch match;

	json encrypted_json;
	for (auto& klass : encrypted_classes) {
		std::regex_search(klass, match, std::regex("(%.*?)\\s"));
		if (!match.size())
			DebugBreak();
		encrypted_offsets.emplace_back(add_class_dump(match[1].str(), klass, dumpData, encrypted_json, true));
	}


#ifndef STAGING
	std::ofstream o("dump\\rust.json");
#else
	std::ofstream o("dump_staging\\rust.json");
#endif
	o << std::setw(4) << final_j << std::endl;

	// create header file
	header(false).dump(final_offsets, fixed_script_offsets);
	header(true).dump(final_offsets, fixed_script_offsets);
	header_encrypted(true).dump(encrypted_offsets);
	csharp().dump(final_offsets, fixed_script_offsets);
	

	// modifying README
#ifndef STAGING
	std::ifstream instream("dump\\README.md");
#else
	std::ifstream instream("dump_staging\\README.md");
#endif
	if (instream.is_open()) {
		std::string readme = slurp(instream);
		instream.close();

		std::regex updexp(R"((\#\#\#\sLast\sUpdate)(\s[\w :-]+))");
		readme = std::regex_replace(readme, updexp, "$1 " + get_current_time());

#ifndef STAGING
		std::ofstream ofs("dump\\README.md");
#else
		std::ofstream ofs("dump_staging\\README.md");
#endif
		ofs << readme;
		ofs.close();
		return;
		std::stringstream ss;
		ss << ":-)" << std::endl;
		std::string replaceexp = "$1" + ss.str() + "$3";
		std::string command = R"(start "" "C:\Program Files\Git\git-bash.exe" -c "perl -i -p0e 's/(##\sIssues\r?\n?)((?:.|\n|\r)*)(##\sCredits:)/")";
		command.append(replaceexp + "/g' 'C:\\Users\\Administrator\\Desktop\\BlazeDumpRust\\dump\\README.md'");
		system(command.c_str());
	}
}

