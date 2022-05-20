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

#include <json.hpp>
#include "xxx.h"
#include "console.hpp"
#include "header.h"
#include "csharp.h"


void update_readme();
void update_check();
void dump_to_github();



std::string exec(const char* cmd) {
	std::array<char, 128> buffer;
	std::string result;
	std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
	if (!pipe) {
		throw std::runtime_error("popen() failed!");
	}
	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
		result += buffer.data();
	}
	return result;
}

std::string remote_build;
std::string local_build;
bool updated = false;

bool update() {
	try {
		console::print_success(true, "Update available");
		console::print(normal, "Local build: ", false);
		console::heck(failure, local_build.c_str());
		console::print(normal, "Remote build: ", false);
		console::heck(success, remote_build.c_str());
		auto response = exec("update_rust.bat");
		if (response.find("Success!") != std::string::npos) {
			updated = true;
			local_build = remote_build;

			std::ofstream outstream("buildid.txt");
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
		console::print_failure(true, "Update failed: %s", e.what());
		return false;
	}
}

void update_check() {

	try {
		static std::regex regexp(R"(^\s\s\s\"public\"\s+\{\s+\"buildid\"\s+\"(\d+)\")");
		auto output = exec("checkver.bat");

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
	/*update_readme();
	return 0;*/
	std::filesystem::path f{ "buildid.txt" };
	if (exists(f)) {
		std::ifstream filein(f);
		filein >> local_build;
		filein.close();
	}
	if (local_build.length())
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
	auto output = exec("dump.bat");
	if (output.find("Done!") != std::string::npos) {
		update_readme();
		exec("script.sh");
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

void basic_scan(std::string classname, std::string& dumpData, offset_parent_t& out) {
	static std::regex regex1(R"((public|private|protected|internal)\s(\w+\.?\w*\.?\w*)\s(\w+);\s\/\/\s(0x[0-9a-fA-F]+)?)"); // works on 90%
	static std::regex regex2(R"((public|private|protected|internal)\s(\w+)\s<(\w+)>.*?;\s\/\/\s(0x[0-9a-fA-F]+))"); // works on stuff like Vector<int>
	static std::regex regexList(R"((public|private|protected|internal)\s(List<.*?>)\s(\w+);\s\/\/\s(0x[0-9a-fA-F]+))"); // only works on real List, append list to name
	static std::regex regexArray(R"((public|private|protected|internal)\s(\w+\[\])\s(.*?);\s\/\/\s(0x[0-9a-fA-F]+))"); // only works on arrays


	// 1. go to start of class
	// 2. shorten data by finding the next class then substr that index
	auto idx = dumpData.find(classname);
	assert(idx != std::string::npos);

	auto nextidx = dumpData.find("// Namespace:", idx + 5);
	assert(nextidx != std::string::npos);

	std::string clazz = dumpData.substr(idx, nextidx - idx);

	for (auto& line : splitbyline(clazz)) {
		std::smatch results;
		if (std::regex_search(line, results, regex1)) {
			out.offsets.emplace_back(offset_entry_t{ results[2].str(), results[3].str(), std::stoul(results[4].str(), nullptr, 16) /* need to convert offset to number */ });
		}
		else if (std::regex_search(line, results, regex2)) {
			out.offsets.emplace_back(offset_entry_t{ results[2].str(), results[3].str(), std::stoul(results[4].str(), nullptr, 16) /* need to convert offset to number */ });
		}
		else if (std::regex_search(line, results, regexList)) {
			bool add = results[2].str().find("List") == std::string::npos;
			out.offsets.emplace_back(offset_entry_t{ results[2].str(), results[3].str() + (add ? "List" : ""), std::stoul(results[4].str(), nullptr, 16) /* need to convert offset to number */ });
		}
		else if (std::regex_search(line, results, regexArray)) {
			out.offsets.emplace_back(offset_entry_t{ results[2].str(), results[3].str(), std::stoul(results[4].str(), nullptr, 16) /* need to convert offset to number */ });
		}
	}
	if (out.offsets.empty())
		DebugBreak();
}

using namespace nlohmann;
void update_readme() {
	std::unordered_map<std::string, std::string> script_offsets = {
		{"BaseEntity_TypeInfo", ""},
		{"MainCamera_TypeInfo", ""},
		{"Facepunch.Input_TypeInfo", ""}
	}; // default values used for script.json
	offset_parent_t baseplayer_offsets = {"BasePlayer"};
	offset_parent_t baseentity_offsets = { "BaseEntity" };
	offset_parent_t BaseCombatEntity_offsets = { "BaseCombatEntity" };
	offset_parent_t BuildingPrivlidge_offsets = { "BuildingPrivlidge" };
	offset_parent_t BaseProjectile_offsets = { "BaseProjectile" };
	offset_parent_t Magazine_offsets = { "Magazine" };
	offset_parent_t PlayerInventory_offsets = { "PlayerInventory" };
	offset_parent_t ItemContainer_offsets = { "ItemContainer" };
	offset_parent_t PlayerModel_offsets = { "PlayerModel" };
	offset_parent_t ModelState_offsets = { "ModelState" };
	offset_parent_t Item_offsets = { "Item" };
	offset_parent_t Model_offsets = { "Model" };

	// fetching values we're interested in
	if (true) {
		std::ifstream script_stream("dump\\script.json");
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
	std::ifstream dump("dump\\dump.cs");
	std::string dumpData = slurp(dump);

	// getting rid of excess data
	auto t = dumpData.find("public class ModelState : IDisposable, Pool.IPooled, IProto");
	assert(t != std::string::npos);
	if (t == std::string::npos) {
		console::print_failure(true, "Failed to find excess splitter");
		return;
	}
	dumpData = dumpData.substr(t - 30);

	// BasePlayer
	basic_scan("public class BasePlayer : BaseCombatEntity, LootPanel.IHasLootPanel", dumpData, baseplayer_offsets);

	// BaseEntity
	basic_scan("public class BaseEntity : BaseNetworkable, IProvider, ILerpTarget, IPrefabPreProcess", dumpData, baseentity_offsets);

	// BaseCombatEntity
	basic_scan("public class BaseCombatEntity : BaseEntity", dumpData, BaseCombatEntity_offsets);

	// BuildingPrivlidge
	basic_scan("public class BuildingPrivlidge : StorageContainer", dumpData, BuildingPrivlidge_offsets);

	// BaseProjectile
	basic_scan("public class BaseProjectile : AttackEntity", dumpData, BaseProjectile_offsets);

	// Magazine
	basic_scan("public class BaseProjectile.Magazine", dumpData, Magazine_offsets);

	// PlayerInventory
	basic_scan("public class PlayerInventory : EntityComponent<BasePlayer>", dumpData, PlayerInventory_offsets);

	// ItemContainer
	basic_scan("public sealed class ItemContainer", dumpData, ItemContainer_offsets);

	// PlayerModel
	basic_scan("public class PlayerModel : ListComponent<PlayerModel>, IOnParentDestroying", dumpData, PlayerModel_offsets);

	// ModelState
	basic_scan("public class ModelState : IDisposable, Pool.IPooled, IProto", dumpData, ModelState_offsets);

	// Item
	basic_scan("public class Item //", dumpData, Item_offsets);

	// Model
	basic_scan("public class Model : MonoBehaviour, IPrefabPreProcess", dumpData, Model_offsets);


	std::vector<offset_parent_t> final_offsets = { baseplayer_offsets , baseentity_offsets, BaseCombatEntity_offsets,
		BuildingPrivlidge_offsets, BaseProjectile_offsets, Magazine_offsets, PlayerInventory_offsets,
	ItemContainer_offsets , PlayerModel_offsets , ModelState_offsets , Item_offsets ,Model_offsets };

	// create json file
	json baseplayer_j;
	for (auto& k : baseplayer_offsets.offsets)
		baseplayer_j[k.name] = k.offset;

	json baseentity_j;
	for (auto& k : baseentity_offsets.offsets)
		baseentity_j[k.name] = k.offset;

	json BaseCombatEntity_j;
	for (auto& k : BaseCombatEntity_offsets.offsets)
		BaseCombatEntity_j[k.name] = k.offset;

	json BuildingPrivlidge_j;
	for (auto& k : BuildingPrivlidge_offsets.offsets)
		BuildingPrivlidge_j[k.name] = k.offset;

	json Baseprojectile_j;
	for (auto& k : BaseProjectile_offsets.offsets)
		Baseprojectile_j[k.name] = k.offset;

	json Magazine_j;
	for (auto& k : Magazine_offsets.offsets)
		Magazine_j[k.name] = k.offset;

	json PlayerInventory_j;
	for (auto& k : PlayerInventory_offsets.offsets)
		PlayerInventory_j[k.name] = k.offset;

	json ItemContainer_j;
	for (auto& k : ItemContainer_offsets.offsets)
		ItemContainer_j[k.name] = k.offset;

	json PlayerModel_j;
	for (auto& k : PlayerModel_offsets.offsets)
		PlayerModel_j[k.name] = k.offset;

	json ModelState_j;
	for (auto& k : ModelState_offsets.offsets)
		ModelState_j[k.name] = k.offset;

	json Item_j;
	for (auto& k : Item_offsets.offsets)
		Item_j[k.name] = k.offset;

	json Model_j;
	for (auto& k : Model_offsets.offsets)
		Model_j[k.name] = k.offset;


	json final_j;
	for (auto& x : script_offsets) {
		assert(x.second.size());
		std::string n = x.first;
		std::replace(n.begin(), n.end(), '.', '_'); // replacing . with _
		final_j[n] = std::stoul(x.second, nullptr, 16);
	}

	final_j["BasePlayer"] = baseplayer_j;
	final_j["BaseEntity"] = baseentity_j;
	final_j["BaseCombatEntity"] = BaseCombatEntity_j;
	final_j["BuildingPrivledge"] = BuildingPrivlidge_j;
	final_j["Magazine"] = Magazine_j;
	final_j["PlayerInventory"] = PlayerInventory_j;
	final_j["ItemContainer"] = ItemContainer_j;
	final_j["PlayerModel"] = PlayerModel_j;
	final_j["ModelState"] = ModelState_j;
	final_j["Item"] = Item_j;
	final_j["Model"] = Model_j;
	final_j["BaseProjectile"] = Baseprojectile_j;

	std::ofstream o("dump\\rust.json");
	o << std::setw(4) << final_j << std::endl;

	// create header file
	header(false).dump(final_offsets, script_offsets);
	header(true).dump(final_offsets, script_offsets);
	csharp().dump(final_offsets, script_offsets);

	// modifying README
	std::ifstream instream("dump\\README.md");
	if (instream.is_open()) {
		std::string readme = slurp(instream);
		instream.close();

		std::regex updexp(R"((\#\#\#\sLast\sUpdate)(\s[\w :-]+))");
		readme = std::regex_replace(readme, updexp, "$1 " + get_current_time());

		std::ofstream ofs("dump\\README.md");
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

