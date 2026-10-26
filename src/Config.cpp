#include "Config.h"

#include <SKSE/SKSE.h>
#include <SimpleIni.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>

namespace bosssense
{
	const char* kConfigPath = "Data/SKSE/Plugins/BossSense.ini";

	namespace
	{
		Settings          g_settings;
		std::mutex        g_mutex;

		std::string Trim(std::string a_s)
		{
			const auto first = a_s.find_first_not_of(" \t\r\n\"");
			if (first == std::string::npos) {
				return {};
			}
			const auto last = a_s.find_last_not_of(" \t\r\n\"");
			return a_s.substr(first, last - first + 1);
		}

		void ReadList(CSimpleIniA& a_ini, const char* a_key, std::vector<std::string>& a_out)
		{
			CSimpleIniA::TNamesDepend values;
			a_ini.GetAllValues("Lists", a_key, values);
			for (const auto& v : values) {
				auto text = Trim(v.pItem ? v.pItem : "");
				if (!text.empty()) {
					a_out.push_back(text);
				}
			}
		}

		void WriteList(CSimpleIniA& a_ini, const char* a_key, const std::vector<std::string>& a_list)
		{
			a_ini.Delete("Lists", a_key, true);
			for (const auto& entry : a_list) {
				a_ini.SetValue("Lists", a_key, entry.c_str(), nullptr, false);
			}
		}

		void SaveSettingsLocked();

		void SetDefaults(Settings& s)
		{
			s.raceForms = {
				"Skyrim.esm:0x012E82",       // DragonRace
				"Skyrim.esm:0x0131EF",       // DragonPriestRace
				"Skyrim.esm:0x0E7713",       // AlduinRace
				"Skyrim.esm:0x0EBE18",       // SkeletonNecroPriestRace
				"Skyrim.esm:0x1052A3",       // UndeadDragonRace
				"Dawnguard.esm:0x00283A",    // DLC1VampireBeastRace
				"Dawnguard.esm:0x010D00",    // DLC1GargoyleVariantBossRace
				"Dawnguard.esm:0x0117DE",    // DLC1UndeadDragonRace
				"Dawnguard.esm:0x015C34",    // DLC1LD_ForgemasterRace
				"Dragonborn.esm:0x01F98F",   // dlc2SpectralDragonRace
				"Dragonborn.esm:0x02C88B",   // DragonBlackRace
				"Dragonborn.esm:0x02C88C",   // DLC2DragonBlackRace
				"Dragonborn.esm:0x03911A",   // DLC2AcolyteDragonPriestRace
				"Dragonborn.esm:0x03CA97",   // DLC2MiraakRace
			};
			s.blacklistForms.clear();
			s.whitelistForms.clear();
		}

		void ReadSettings()
		{
			CSimpleIniA ini;
			ini.SetUnicode();
			ini.SetMultiKey();

			const auto rc = ini.LoadFile(kConfigPath);
			if (rc < 0) {
				SKSE::log::info("BossSense.ini not found, writing defaults");
				SetDefaults(g_settings);
				SaveSettingsLocked();
				return;
			}

			g_settings.enabled       = ini.GetBoolValue("General", "Enabled", g_settings.enabled);
			g_settings.onlyHostile   = ini.GetBoolValue("General", "OnlyHostile", g_settings.onlyHostile);
			g_settings.requireCombat = ini.GetBoolValue("General", "RequireCombat", g_settings.requireCombat);
			g_settings.debugLog      = ini.GetBoolValue("General", "DebugLog", g_settings.debugLog);

			g_settings.threshold  = static_cast<int>(ini.GetLongValue("Thresholds", "Threshold", g_settings.threshold));
			g_settings.levelDelta = static_cast<int>(ini.GetLongValue("Thresholds", "LevelDelta", g_settings.levelDelta));
			g_settings.healthMin  = static_cast<float>(ini.GetDoubleValue("Thresholds", "HealthMin", g_settings.healthMin));

			auto readSignal = [&](const char* a_key, Signal& a_sig) {
				a_sig.weight  = static_cast<int>(ini.GetLongValue("Weights", a_key, a_sig.weight));
				a_sig.enabled = a_sig.weight > 0;
			};
			readSignal("Race", g_settings.race);
			readSignal("LocRef", g_settings.locRef);
			readSignal("Edid", g_settings.edid);
			readSignal("Unique", g_settings.unique);
			readSignal("Level", g_settings.level);
			readSignal("Health", g_settings.health);
			readSignal("Aidt", g_settings.aidt);
			readSignal("Named", g_settings.named);
			readSignal("CustomName", g_settings.custom);
			readSignal("NotLeveled", g_settings.notLeveled);

			g_settings.raceForms.clear();
			g_settings.blacklistForms.clear();
			g_settings.whitelistForms.clear();
			ReadList(ini, "BossRace", g_settings.raceForms);
			ReadList(ini, "Blacklist", g_settings.blacklistForms);
			ReadList(ini, "Whitelist", g_settings.whitelistForms);

			if (g_settings.raceForms.empty()) {
				Settings defaults;
				SetDefaults(defaults);
				g_settings.raceForms = defaults.raceForms;
			}

			SKSE::log::info("settings loaded: threshold={}", g_settings.threshold);
		}
	}

	std::mutex& SettingsMutex()
	{
		return g_mutex;
	}

	Settings& MutableSettings()
	{
		return g_settings;
	}

	SettingsSnapshot MakeSnapshot()
	{
		std::lock_guard lock(g_mutex);
		SettingsSnapshot s;
		s.enabled       = g_settings.enabled;
		s.onlyHostile   = g_settings.onlyHostile;
		s.requireCombat = g_settings.requireCombat;
		s.debugLog      = g_settings.debugLog;
		s.threshold     = g_settings.threshold;
		s.levelDelta    = g_settings.levelDelta;
		s.healthMin     = g_settings.healthMin;
		s.race          = g_settings.race;
		s.locRef        = g_settings.locRef;
		s.edid          = g_settings.edid;
		s.unique        = g_settings.unique;
		s.level         = g_settings.level;
		s.health        = g_settings.health;
		s.aidt          = g_settings.aidt;
		s.named         = g_settings.named;
		s.bossRaces     = g_settings.bossRaces;
		s.blacklist     = g_settings.blacklist;
		s.whitelist     = g_settings.whitelist;
		return s;
	}

	void LoadSettings()
	{
		std::lock_guard lock(g_mutex);
		ReadSettings();
	}

	namespace
	{
		void SaveSettingsLocked()
		{
			CSimpleIniA ini;
			ini.SetUnicode();
			ini.SetMultiKey();

			ini.SetBoolValue("General", "Enabled", g_settings.enabled);
			ini.SetBoolValue("General", "OnlyHostile", g_settings.onlyHostile);
			ini.SetBoolValue("General", "RequireCombat", g_settings.requireCombat);
			ini.SetBoolValue("General", "DebugLog", g_settings.debugLog);

			ini.SetLongValue("Thresholds", "Threshold", g_settings.threshold);
			ini.SetLongValue("Thresholds", "LevelDelta", g_settings.levelDelta);
			ini.SetDoubleValue("Thresholds", "HealthMin", g_settings.healthMin);

			ini.SetLongValue("Weights", "Race", g_settings.race.enabled ? g_settings.race.weight : 0);
			ini.SetLongValue("Weights", "LocRef", g_settings.locRef.enabled ? g_settings.locRef.weight : 0);
			ini.SetLongValue("Weights", "Edid", g_settings.edid.enabled ? g_settings.edid.weight : 0);
			ini.SetLongValue("Weights", "Unique", g_settings.unique.enabled ? g_settings.unique.weight : 0);
			ini.SetLongValue("Weights", "Level", g_settings.level.enabled ? g_settings.level.weight : 0);
			ini.SetLongValue("Weights", "Health", g_settings.health.enabled ? g_settings.health.weight : 0);
			ini.SetLongValue("Weights", "Aidt", g_settings.aidt.enabled ? g_settings.aidt.weight : 0);
			ini.SetLongValue("Weights", "Named", g_settings.named.enabled ? g_settings.named.weight : 0);
			ini.SetLongValue("Weights", "CustomName", g_settings.custom.enabled ? g_settings.custom.weight : 0);
			ini.SetLongValue("Weights", "NotLeveled", g_settings.notLeveled.enabled ? g_settings.notLeveled.weight : 0);

			WriteList(ini, "BossRace", g_settings.raceForms);
			WriteList(ini, "Blacklist", g_settings.blacklistForms);
			WriteList(ini, "Whitelist", g_settings.whitelistForms);

			std::error_code ec;
			std::filesystem::create_directories(std::filesystem::path(kConfigPath).parent_path(), ec);

			if (ini.SaveFile(kConfigPath) < 0) {
				SKSE::log::error("failed to save BossSense.ini");
			}
			g_settings.dirty = false;
		}
	}

	void SaveSettings()
	{
		std::lock_guard lock(g_mutex);
		SaveSettingsLocked();
	}

	void ResolveFormLists()
	{
		std::lock_guard lock(g_mutex);

		auto resolve = [](const std::vector<std::string>& a_entries, std::unordered_set<RE::FormID>& a_out) {
			a_out.clear();
			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler) {
				return;
			}
			for (const auto& entry : a_entries) {
				const auto split = entry.find(':');
				if (split == std::string::npos) {
					SKSE::log::warn("bad form entry '{}' (expected Plugin.esp:0x123456)", entry);
					continue;
				}
				const auto plugin = entry.substr(0, split);
				const auto idText = entry.substr(split + 1);
				RE::FormID local = 0;
				try {
					local = static_cast<RE::FormID>(std::stoul(idText, nullptr, 0));
				} catch (...) {
					SKSE::log::warn("bad form id in '{}'", entry);
					continue;
				}
				if (auto* form = dataHandler->LookupForm(local, plugin)) {
					a_out.insert(form->GetFormID());
				} else {
					SKSE::log::warn("form not found: {}", entry);
				}
			}
		};

		resolve(g_settings.raceForms, g_settings.bossRaces);
		resolve(g_settings.blacklistForms, g_settings.blacklist);
		resolve(g_settings.whitelistForms, g_settings.whitelist);

		SKSE::log::info("forms resolved: {} boss races, {} blacklist, {} whitelist",
			g_settings.bossRaces.size(), g_settings.blacklist.size(), g_settings.whitelist.size());
	}
}
