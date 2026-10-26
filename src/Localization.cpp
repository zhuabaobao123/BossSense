#include "Localization.h"

#include <SKSE/SKSE.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace
{
	std::unordered_map<std::string, std::string> g_entries;
	bool                                         g_loaded = false;
}

void Localization::Load()
{
	if (g_loaded) {
		return;
	}
	g_loaded = true;

	wchar_t modulePath[MAX_PATH]{};
	const auto self = GetModuleHandleW(L"BossSense.dll");
	if (!self) {
		return;
	}
	if (GetModuleFileNameW(self, modulePath, MAX_PATH) == 0) {
		return;
	}

	std::filesystem::path jsonPath(modulePath);
	jsonPath.replace_extension(L".json");

	std::ifstream in(jsonPath, std::ios::binary);
	if (!in) {
		SKSE::log::info("no translation file ({}) - English UI", jsonPath.filename().string());
		return;
	}

	std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (text.size() >= 3 &&
		static_cast<unsigned char>(text[0]) == 0xEF &&
		static_cast<unsigned char>(text[1]) == 0xBB &&
		static_cast<unsigned char>(text[2]) == 0xBF) {
		text.erase(0, 3);
	}

	try {
		const auto json = nlohmann::json::parse(text);
		if (!json.is_object()) {
			SKSE::log::error("translation file is not a JSON object");
			return;
		}
		for (auto it = json.begin(); it != json.end(); ++it) {
			if (it.value().is_string()) {
				g_entries[it.key()] = it.value().get<std::string>();
			}
		}
	} catch (const std::exception& e) {
		SKSE::log::error("translation file parse error: {}", e.what());
		return;
	}

	SKSE::log::info("translations loaded: {} entries ({})", g_entries.size(), jsonPath.filename().string());
}

const char* Localization::Translate(const char* a_key)
{
	if (!a_key) {
		return "";
	}
	const auto it = g_entries.find(a_key);
	return it != g_entries.end() ? it->second.c_str() : a_key;
}
