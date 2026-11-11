#include "Distribution.h"

#include "Config.h"

#include <SKSE/SKSE.h>

#include <algorithm>

namespace bosssense::distribution
{
	namespace
	{
		struct ItemEntry
		{
			RE::TESBoundObject* object = nullptr;
			int                 count = 1;
		};

		std::vector<ItemEntry>      g_items;
		std::vector<RE::SpellItem*> g_spells;
		std::vector<RE::BGSPerk*>   g_perks;

		std::string Trim(std::string a_s)
		{
			const auto first = a_s.find_first_not_of(" \t\r\n\"");
			if (first == std::string::npos) {
				return {};
			}
			const auto last = a_s.find_last_not_of(" \t\r\n\"");
			return a_s.substr(first, last - first + 1);
		}

		/// Resolve "Plugin.esp:0x123456" (and an optional "|count" for items).
		RE::TESForm* Resolve(const std::string& a_entry, int& a_count)
		{
			a_count = 1;

			auto text = Trim(a_entry);
			if (text.empty()) {
				return nullptr;
			}

			if (const auto bar = text.find('|'); bar != std::string::npos) {
				try {
					a_count = std::max(1, std::stoi(text.substr(bar + 1)));
				} catch (...) {
					a_count = 1;
				}
				text = Trim(text.substr(0, bar));
			}

			const auto colon = text.find(':');
			if (colon == std::string::npos) {
				SKSE::log::warn("distribution: bad entry '{}' (expected Plugin.esp:0x123456)", a_entry);
				return nullptr;
			}

			RE::FormID local = 0;
			try {
				local = static_cast<RE::FormID>(std::stoul(text.substr(colon + 1), nullptr, 0));
			} catch (...) {
				SKSE::log::warn("distribution: bad form id in '{}'", a_entry);
				return nullptr;
			}

			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			auto* form = dataHandler ? dataHandler->LookupForm(local, text.substr(0, colon)) : nullptr;
			if (!form) {
				SKSE::log::warn("distribution: form not found '{}'", a_entry);
			}
			return form;
		}
	}

	void Init()
	{
		g_items.clear();
		g_spells.clear();
		g_perks.clear();

		std::vector<std::string> items;
		std::vector<std::string> spells;
		std::vector<std::string> perks;
		{
			std::lock_guard lock(SettingsMutex());
			const auto& cfg = MutableSettings();
			items = cfg.itemForms;
			spells = cfg.spellForms;
			perks = cfg.perkForms;
		}

		int count = 1;
		for (const auto& entry : items) {
			auto* form = Resolve(entry, count);
			if (!form) {
				continue;
			}
			if (auto* object = form->As<RE::TESBoundObject>()) {
				g_items.push_back({ object, count });
			} else {
				SKSE::log::warn("distribution: '{}' is not an item", entry);
			}
		}

		for (const auto& entry : spells) {
			auto* form = Resolve(entry, count);
			if (!form) {
				continue;
			}
			if (auto* spell = form->As<RE::SpellItem>()) {
				g_spells.push_back(spell);
			} else {
				SKSE::log::warn("distribution: '{}' is not a spell", entry);
			}
		}

		for (const auto& entry : perks) {
			auto* form = Resolve(entry, count);
			if (!form) {
				continue;
			}
			if (auto* perk = form->As<RE::BGSPerk>()) {
				g_perks.push_back(perk);
			} else {
				SKSE::log::warn("distribution: '{}' is not a perk", entry);
			}
		}

		if (!g_items.empty() || !g_spells.empty() || !g_perks.empty()) {
			SKSE::log::info("distribution: {} item(s), {} spell(s), {} perk(s) configured", g_items.size(),
				g_spells.size(), g_perks.size());
		}
	}

	void Give(RE::Actor* a_actor, bool a_items, bool a_spells, bool a_perks)
	{
		if (!a_actor || (!a_items && !a_spells && !a_perks)) {
			return;
		}

		const auto formID = a_actor->GetFormID();

		if (a_items) {
			auto* changes = a_actor->GetInventoryChanges();
			for (const auto& entry : g_items) {
				if (!entry.object) {
					continue;
				}
				const auto have = changes ? static_cast<int>(changes->GetItemCount(entry.object)) : 0;
				if (have >= entry.count) {
					continue;
				}
				a_actor->AddObjectToContainer(entry.object, nullptr, entry.count - have, nullptr);
				SKSE::log::info("distribution: [{:08X}] {} x{}", formID, entry.object->GetName(), entry.count - have);
			}
		}

		if (a_spells) {
			for (auto* spell : g_spells) {
				if (!spell || a_actor->HasSpell(spell)) {
					continue;
				}
				if (a_actor->AddSpell(spell)) {
					SKSE::log::info("distribution: [{:08X}] spell {}", formID, spell->GetName());
				}
			}
		}

		if (a_perks) {
			for (auto* perk : g_perks) {
				if (!perk || a_actor->HasPerk(perk)) {
					continue;
				}
				a_actor->AddPerk(perk);
				SKSE::log::info("distribution: [{:08X}] perk {}", formID, perk->GetName());
			}
		}
	}
}
