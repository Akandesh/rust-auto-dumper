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
		if (log_stream != nullptr && log_stream->good() && log_stream->is_open())
			*log_stream << "[" << time << "] " << data;
	}
	if (log_stream != nullptr && log_stream->good() && log_stream->is_open()) {
		log_stream->close();
	}
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
	static std::regex regex1(R"((public|private|protected|internal)\s(\w+\.?\w*\.?\w*)\s(\w+);\s\/\/\s(0x[0-9a-fA-F]+)?)"); // works on 90%
	static std::regex regex2(R"((public|private|protected|internal)\s(\w+\.?\w.*)\s<(\w+)>.*?;\s\/\/\s(0x[0-9a-fA-F]+))"); // works on stuff like Vector<int> and thing.thing<thing>
	static std::regex regexList(R"((public|private|protected|internal)\s(List<.*?>)\s(\w+);\s\/\/\s(0x[0-9a-fA-F]+))"); // only works on real List, append list to name
	static std::regex regexArray(R"((public|private|protected|internal)\s(\w+\[\])\s(.*?);\s\/\/\s(0x[0-9a-fA-F]+))"); // only works on arrays
	static std::regex regexEntityRef(R"((public|private|protected|internal)\s(EntityRef<\w+>)\s(\w+);\s\/\/\s(0x[0-9a-fA-F]+)?)"); // only works on entityrefs
	static std::regex regexStatic(R"((public|private|protected|internal)\sstatic\s(\w+\.?\w*\.?\w*)\s(\w+);\s\/\/\s(0x[0-9a-fA-F]+)?)");

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
		else if (std::regex_search(line, results, regexEntityRef)) {
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

using namespace nlohmann;
void update_readme() {
	console::set_title_message("Parsing dump..");
	std::unordered_map<std::string, std::string> script_offsets = {
		{"BaseEntity_TypeInfo", ""},
		{"MainCamera_TypeInfo", ""},
		{"Facepunch.Input_TypeInfo", ""},
		{"System.Collections.Generic.List<BaseGameMode>_TypeInfo", ""},
		{"BaseGameMode_TypeInfo", ""}
	}; // default values used for script.json
	offset_parent_t baseplayer_offsets = { "BasePlayer" };
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
	offset_parent_t RecoilProperties_offsets = { "RecoilProperties" };
	offset_parent_t BaseFishingRod_offsets = { "BaseFishingRod" };
	offset_parent_t FishingBobber_offsets = { "FishingBobber" };
	offset_parent_t OcclusionCulling_offsets = { "OcclusionCulling" };
	offset_parent_t OcclusionCulling_DebugSettings_offsets = { "OcclusionCulling_DebugSettings" };


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
	std::list<std::string> all_classes = {
	"public class BasePlayer : BaseCombatEntity, LootPanel.IHasLootPanel",
	"public class BaseEntity : BaseNetworkable, IProvider",
	"public class BaseCombatEntity : BaseEntity",
	"public class BuildingPrivlidge : StorageContainer",
	"public class BaseProjectile : AttackEntity",
	"public class BaseProjectile.Magazine",
	"public class PlayerInventory : EntityComponent<BasePlayer>",
	"public sealed class ItemContainer",
	"public class PlayerModel : ListComponent<PlayerModel>, IOnParentDestroying",
	"public class ModelState : IDisposable, Pool.IPooled, IProto",
	"public class Item //",
	"public class Model : MonoBehaviour, IPrefabPreProcess",
	"public class RecoilProperties : ScriptableObject",
	"public class BaseFishingRod : HeldEntity",
	"public class FishingBobber : BaseCombatEntity",
	"public class OcclusionCulling : MonoBehaviour",
	"public class OcclusionCulling.DebugSettings"
	};

	std::string first_class = "";
	for (auto& klass : all_classes) {
		static size_t lowest_idx = INT_MAX;
		size_t index = dumpData.find(klass);
		if (index < lowest_idx) {
			lowest_idx = index;
			first_class = klass;
		}
	}

	printf("Best class is %s\n", first_class.c_str());
#endif
	dumpData = dumpData.substr(t - 30);

	// BasePlayer
	basic_scan("public class BasePlayer : BaseCombatEntity, LootPanel.IHasLootPanel", dumpData, baseplayer_offsets);

	// BaseEntity
	basic_scan("public class BaseEntity : BaseNetworkable, IProvider", dumpData, baseentity_offsets);

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

	// RecoilProperties
	basic_scan("public class RecoilProperties : ScriptableObject", dumpData, RecoilProperties_offsets);

	// BaseFishingRod
	basic_scan("public class BaseFishingRod : HeldEntity", dumpData, BaseFishingRod_offsets);

	// FishingBobber
	basic_scan("public class FishingBobber : BaseCombatEntity", dumpData, FishingBobber_offsets);

	// OcclusionCulling
	basic_scan("public class OcclusionCulling : MonoBehaviour", dumpData, OcclusionCulling_offsets, true);

	// OcclusionCulling.DebugSettings
	basic_scan("public class OcclusionCulling.DebugSettings", dumpData, OcclusionCulling_DebugSettings_offsets);


	std::vector<offset_parent_t> final_offsets = { baseplayer_offsets , baseentity_offsets, BaseCombatEntity_offsets,
		BuildingPrivlidge_offsets, BaseProjectile_offsets, Magazine_offsets, PlayerInventory_offsets,
	ItemContainer_offsets , PlayerModel_offsets , ModelState_offsets , Item_offsets ,Model_offsets, RecoilProperties_offsets, BaseFishingRod_offsets,
	FishingBobber_offsets, OcclusionCulling_offsets, OcclusionCulling_DebugSettings_offsets };

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

	json Recooil_j;
	for (auto& k : RecoilProperties_offsets.offsets)
		Recooil_j[k.name] = k.offset;

	json BaseFishingRod_j;
	for (auto& k : BaseFishingRod_offsets.offsets)
		BaseFishingRod_j[k.name] = k.offset;

	json FishingBobber_j;
	for (auto& k : FishingBobber_offsets.offsets)
		FishingBobber_j[k.name] = k.offset;

	json OcclusionCulling_j;
	for (auto& k : OcclusionCulling_offsets.offsets)
		OcclusionCulling_j[k.name] = k.offset;

	json OcclusionCulling_DebugSettings_j;
	for (auto& k : OcclusionCulling_DebugSettings_offsets.offsets)
		OcclusionCulling_DebugSettings_j[k.name] = k.offset;

	json final_j;
	std::unordered_map<std::string, std::string> fixed_script_offsets = { };
	for (auto& x : script_offsets) {
		assert(x.second.size());
		std::string n = x.first;
		std::replace(n.begin(), n.end(), '.', '_'); // replacing . with _
		std::replace(n.begin(), n.end(), '<', '_'); // replacing < with _
		n.erase(std::remove(n.begin(), n.end(), '>')); // removing >
		fixed_script_offsets.insert({ n, x.second });
		final_j[n] = std::atoi(x.second.c_str());
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
	final_j["RecoilProperties"] = Recooil_j;
	final_j["BaseFishingRod"] = BaseFishingRod_j;
	final_j["FishingBobber"] = FishingBobber_j;
	final_j["OcclusionCulling"] = OcclusionCulling_j;
	final_j["OcclusionCulling.DebugSettings"] = OcclusionCulling_DebugSettings_j;

#ifndef STAGING
	std::ofstream o("dump\\rust.json");
#else
	std::ofstream o("dump_staging\\rust.json");
#endif
	o << std::setw(4) << final_j << std::endl;

	// create header file
	header(false).dump(final_offsets, fixed_script_offsets);
	header(true).dump(final_offsets, fixed_script_offsets);
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

