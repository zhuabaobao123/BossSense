#include "BossManager.h"

#include "Config.h"
#include "Detector.h"
#include "TrueHUD.h"

#include <SKSE/SKSE.h>

namespace bosssense
{
	BossManager& BossManager::Get()
	{
		static BossManager instance;
		return instance;
	}

	void BossManager::Register()
	{
		auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
		if (!holder) {
			SKSE::log::error("ScriptEventSourceHolder missing; no events registered");
			return;
		}
		holder->AddEventSink<RE::TESCombatEvent>(this);
		holder->AddEventSink<RE::TESDeathEvent>(this);
		holder->AddEventSink<RE::TESHitEvent>(this);
		SKSE::log::info("event sinks registered");
	}

	void BossManager::Reset()
	{
		m_added.clear();
		m_decided.clear();
		m_sessionCount = 0;
		SKSE::log::info("boss tracking reset");
	}

	Decision BossManager::EvaluateOnly(RE::Actor* a_actor)
	{
		return Evaluate(a_actor);
	}

	std::string BossManager::LastSummary()
	{
		std::lock_guard lock(m_summaryMutex);
		return m_lastSummary;
	}

	void BossManager::Forget(RE::FormID a_formID)
	{
		m_added.erase(a_formID);
		m_decided.erase(a_formID);
	}

	void BossManager::Invalidate()
	{
		m_decided.clear();
	}

	void BossManager::ReevaluateAll()
	{
		m_decided.clear();

		// Only touch actors we added a bar for. ProcessLists is the whole world, not "nearby",
		// so iterating it would add bars to thousands of actors.
		std::vector<RE::FormID> tracked(m_added.begin(), m_added.end());
		for (const auto formID : tracked) {
			auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
			if (!actor) {
				m_added.erase(formID);
				continue;
			}

			const auto decision = Evaluate(actor);
			if (!decision.boss) {
				truehud::RemoveBoss(actor, true);
				m_added.erase(formID);
				SKSE::log::info("boss -{} [{:08X}] {}: score {} < {}", decision.score, formID,
					decision.name.empty() ? "<unnamed>" : decision.name.c_str(), decision.score, decision.threshold);
			}
		}

		SKSE::log::info("re-evaluated {} tracked boss bars, {} left", tracked.size(), m_added.size());
	}

	void BossManager::ConsiderNow(RE::Actor* a_actor)
	{
		Consider(a_actor);
	}

	void BossManager::RemoveTracked(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		const auto formID = a_actor->GetFormID();
		truehud::RemoveBoss(a_actor, true);
		m_added.erase(formID);
		m_decided.erase(formID);
		SKSE::log::info("boss - [{:08X}] removed by blacklist", formID);
	}

	void BossManager::Consider(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		const auto formID = a_actor->GetFormID();
		const bool alreadyAdded = m_added.contains(formID);

		// Actors without a bar are decided once per combat; actors that already have a bar are
		// re-checked on every event so a score drop can remove the bar.
		if (!alreadyAdded && m_decided.contains(formID)) {
			return;
		}
		m_decided.insert(formID);

		const auto snapshot = MakeSnapshot();
		auto decision = Evaluate(a_actor);

		const bool debug = snapshot.debugLog;
		if (debug || (decision.boss && !alreadyAdded) || (!decision.boss && alreadyAdded)) {
			std::string reasons;
			for (int i = 0; i < decision.hitCount; ++i) {
				if (!reasons.empty()) {
					reasons += "+";
				}
				reasons += decision.hits[i].name;
			}

			const char* name = decision.name.empty() ? "<unnamed>" : decision.name.c_str();
			if (decision.boss && !alreadyAdded) {
				SKSE::log::info("boss +{} [{:08X}] base={:08X} {} ({}): {}", decision.score, formID, decision.baseID, name,
					decision.edid.empty() ? "-" : decision.edid.c_str(), reasons);
			} else if (!decision.boss && alreadyAdded) {
				SKSE::log::info("boss -{} [{:08X}] base={:08X} {}: score {} < {} ({})", decision.score, formID, decision.baseID, name,
					decision.score, decision.threshold, reasons.empty() ? "no signal" : reasons.c_str());
			} else if (debug) {
				SKSE::log::info("skip {} [{:08X}] base={:08X} {}: score {} < {} ({})", name, formID, decision.baseID,
					decision.edid.empty() ? "-" : decision.edid.c_str(), decision.score,
					decision.threshold, reasons.empty() ? "no signal" : reasons.c_str());
			}
		}

		if (!decision.boss) {
			if (alreadyAdded) {
				truehud::RemoveBoss(a_actor);
				m_added.erase(formID);
			}
			return;
		}

		if (alreadyAdded) {
			return;
		}

		m_added.insert(formID);
		if (truehud::Available()) {
			truehud::AddBoss(a_actor);
		}
		++m_sessionCount;

		std::lock_guard lock(m_summaryMutex);
		m_lastSummary = std::string(decision.name.empty() ? "?" : decision.name) + " (" +
		                std::to_string(decision.score) + ")";
	}

	RE::BSEventNotifyControl BossManager::ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>*)
	{
		if (!a_event || !a_event->actor) {
			return RE::BSEventNotifyControl::kContinue;
		}

		auto* actor = a_event->actor->As<RE::Actor>();
		if (!actor) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (a_event->newState == RE::ACTOR_COMBAT_STATE::kCombat) {
			Consider(actor);
		} else {
			// TrueHUD removes boss bars on combat end; allow a fresh add next time.
			Forget(actor->GetFormID());
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl BossManager::ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		if (!a_event || !a_event->actorDying || !a_event->dead) {
			return RE::BSEventNotifyControl::kContinue;
		}
		if (auto* actor = a_event->actorDying->As<RE::Actor>()) {
			Forget(actor->GetFormID());
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl BossManager::ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*)
	{
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (a_event->target) {
			if (auto* actor = a_event->target->As<RE::Actor>()) {
				Consider(actor);
			}
		}
		if (a_event->cause) {
			if (auto* actor = a_event->cause->As<RE::Actor>()) {
				Consider(actor);
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}
}
