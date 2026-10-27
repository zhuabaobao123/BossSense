#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace bosssense
{
	struct Signal
	{
		bool enabled = true;
		int  weight = 50;
	};

	struct Settings
	{
		bool enabled = true;
		bool onlyHostile = true;
		bool requireCombat = false;
		bool debugLog = false;

		int   threshold = 70;
		int   levelDelta = 5;
		int   levelMin = 30;
		float healthMin = 400.0f;

		Signal race{ true, 100 };
		Signal locRef{ true, 100 };
		Signal edid{ true, 90 };
		Signal unique{ true, 20 };
		Signal level{ true, 20 };
		Signal highLevel{ true, 15 };
		Signal health{ true, 0 };
		Signal aidt{ true, 10 };
		Signal named{ true, 0 };
		Signal custom{ true, 25 };
		Signal notLeveled{ true, 20 };

		// Raw config text, kept verbatim so a save never loses what the user wrote.
		std::vector<std::string> raceForms;
		std::vector<std::string> blacklistForms;
		std::vector<std::string> whitelistForms;
		std::vector<std::string> removeBlacklistForms;
		std::vector<std::string> removeWhitelistForms;

		// Resolved forms
		std::unordered_set<RE::FormID> bossRaces;
		std::unordered_set<RE::FormID> blacklist;
		std::unordered_set<RE::FormID> whitelist;

		bool dirty = false;
	};

	/// Copy of the tunables, safe to read from the game thread while the UI edits them.
	struct SettingsSnapshot
	{
		bool  enabled = true;
		bool  onlyHostile = true;
		bool  requireCombat = false;
		bool  debugLog = false;
		int   threshold = 70;
		int   levelDelta = 5;
		int   levelMin = 30;
		float healthMin = 400.0f;

		Signal race;
		Signal locRef;
		Signal edid;
		Signal unique;
		Signal level;
		Signal highLevel;
		Signal health;
		Signal aidt;
		Signal named;
		Signal custom;
		Signal notLeveled;

		std::unordered_set<RE::FormID> bossRaces;
		std::unordered_set<RE::FormID> blacklist;
		std::unordered_set<RE::FormID> whitelist;
	};

	std::mutex&       SettingsMutex();
	Settings&         MutableSettings();
	SettingsSnapshot  MakeSnapshot();

	void LoadSettings();
	void SaveSettings();
	/// Resolve the "Plugin.esp:0x123456" lists; call once after kDataLoaded.
	void ResolveFormLists();

	extern const char* kConfigPath;
}
