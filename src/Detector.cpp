#include "Detector.h"

#include "Config.h"
#include "PluginScanner.h"

#include <SKSE/SKSE.h>

namespace bosssense
{
	namespace
	{
		RE::FormID g_bossLocRef = 0;      // Skyrim.esm:0x000130F7 "Boss"
		RE::FormID g_bossLocRefDLC2 = 0;  // Dragonborn.esm:0x000206B5 "DLC2Boss1"

		RE::ExtraLeveledCreature* GetLeveledCreature(RE::Actor* a_actor)
		{
			return static_cast<RE::ExtraLeveledCreature*>(
				a_actor->extraList.GetByType(RE::ExtraDataType::kLeveledCreature));
		}
	}

	void InitDetectorForms()
	{
		if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
			if (auto* form = dataHandler->LookupForm(0x000130F7, "Skyrim.esm")) {
				g_bossLocRef = form->GetFormID();
			}
			if (auto* form = dataHandler->LookupForm(0x000206B5, "Dragonborn.esm")) {
				g_bossLocRefDLC2 = form->GetFormID();
			}
		}
		SKSE::log::info("boss loc ref types: {:08X} {:08X}", g_bossLocRef, g_bossLocRefDLC2);
	}

	Decision Evaluate(RE::Actor* a_actor)
	{
		Decision d;
		const auto s = MakeSnapshot();
		if (!s.enabled) {
			return d;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!a_actor || !player || a_actor == player || a_actor->IsPlayerRef()) {
			return d;
		}
		if (a_actor->IsDead()) {
			return d;
		}
		if (a_actor->IsPlayerTeammate() || a_actor->IsSummonedByPlayer() || a_actor->IsSummoned()) {
			return d;
		}
		if (s.onlyHostile && !a_actor->IsHostileToActor(player)) {
			return d;
		}
		if (s.requireCombat && !a_actor->IsInCombat()) {
			return d;
		}
		if (a_actor->IsEssential() && a_actor->AsActorState()->IsBleedingOut()) {
			return d;
		}

		auto* base = a_actor->GetActorBase();
		if (!base) {
			return d;
		}

		d.baseID = base->GetFormID();
		if (const char* fullName = base->GetFullName()) {
			d.name = fullName;
		}

		RE::FormID originalID = 0;
		if (auto* leveled = GetLeveledCreature(a_actor)) {
			if (leveled->originalBase) {
				originalID = leveled->originalBase->GetFormID();
			}
		}

		const scanner::NpcInfo* baseInfo = scanner::Find(d.baseID);
		const scanner::NpcInfo* originalInfo = originalID ? scanner::Find(originalID) : nullptr;
		if (baseInfo && !baseInfo->edid.empty()) {
			d.edid = baseInfo->edid;
		} else if (originalInfo) {
			d.edid = originalInfo->edid;
		}

		if (s.blacklist.contains(d.baseID) || (originalID && s.blacklist.contains(originalID))) {
			d.Blacklisted = true;
			return d;
		}
		if (s.whitelist.contains(d.baseID) || (originalID && s.whitelist.contains(originalID))) {
			d.boss = true;
			d.Whitelisted = true;
			d.score = 999;
			return d;
		}

		auto add = [&d](const char* a_name, const Signal& a_signal) {
			if (a_signal.enabled && a_signal.weight > 0) {
				d.hits[d.hitCount++] = { a_name, a_signal.weight };
				d.score += a_signal.weight;
			}
		};

		// 1. Race whitelist (dragons, dragon priests, ...)
		if (s.race.enabled && s.race.weight > 0) {
			if (auto* race = a_actor->GetRace(); race && s.bossRaces.contains(race->GetFormID())) {
				add("race", s.race);
			}
		}

		// 2. Location ref type "Boss" (dungeon boss markers)
		if (s.locRef.enabled && s.locRef.weight > 0 && (g_bossLocRef || g_bossLocRefDLC2)) {
			if (auto* location = player->GetPlayerRuntimeData().currentLocation) {
				for (auto& ref : location->specialRefs) {
					if (!ref.type) {
						continue;
					}
					const auto typeID = ref.type->GetFormID();
					if ((typeID == g_bossLocRef || typeID == g_bossLocRefDLC2) && ref.refData.refID == a_actor->GetFormID()) {
						add("locRefType", s.locRef);
						break;
					}
				}
			}
		}

		// 3. EditorID / original base EditorID contains "boss"
		if (s.edid.enabled && s.edid.weight > 0) {
			if ((baseInfo && baseInfo->edidBoss) || (originalInfo && originalInfo->edidBoss)) {
				add("edid", s.edid);
			}
		}

		// 4. Hand-authored NPC (a real EditorID, not a Lvl/Enc template)
		if (s.custom.enabled && s.custom.weight > 0) {
			if ((baseInfo && baseInfo->customName) || (originalInfo && originalInfo->customName)) {
				add("customName", s.custom);
			}
		}

		// 5. Not spawned from a leveled list: placed by hand or a fixed quest actor
		if (s.notLeveled.enabled && s.notLeveled.weight > 0) {
			if (!originalID) {
				add("notLeveled", s.notLeveled);
			}
		}

		// 4. Unique flag
		if (s.unique.enabled && s.unique.weight > 0 && base->IsUnique()) {
			add("unique", s.unique);
		}

		// 5. Level relative to the player
		if (s.level.enabled && s.level.weight > 0) {
			if (a_actor->GetLevel() >= player->GetLevel() + s.levelDelta) {
				add("level", s.level);
			}
		}

		// 6. Health pool
		if (s.health.enabled && s.health.weight > 0) {
			if (a_actor->GetActorValueMax(RE::ActorValue::kHealth) >= s.healthMin) {
				add("health", s.health);
			}
		}

		// 7. AIDT: very aggressive + foolhardy
		if (s.aidt.enabled && s.aidt.weight > 0) {
			const auto aggression = base->GetAggressionLevel();
			const auto confidence = base->GetConfidenceLevel();
			if (aggression >= RE::ACTOR_AGGRESSION::kVeryAggressive && confidence == RE::ACTOR_CONFIDENCE::kFoolhardy) {
				add("aidt", s.aidt);
			} else if (const scanner::NpcInfo* info = scanner::Find(d.baseID); info && info->aidtBoss) {
				add("aidt", s.aidt);
			}
		}

		// 8. Has a proper name
		if (s.named.enabled && s.named.weight > 0 && !d.name.empty()) {
			add("named", s.named);
		}

		d.boss = d.score >= s.threshold;
		d.threshold = s.threshold;
		return d;
	}
}

