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

	void BossManager::ConsiderNow(RE::Actor* a_actor)
	{
		Consider(a_actor);
	}

	void BossManager::Consider(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		const auto formID = a_actor->GetFormID();
		if (m_decided.contains(formID)) {
			return;
		}
		m_decided.insert(formID);

		auto decision = Evaluate(a_actor);

		const bool debug = MakeSnapshot().debugLog;
		if (debug || decision.boss) {
			std::string reasons;
			for (int i = 0; i < decision.hitCount; ++i) {
				if (!reasons.empty()) {
					reasons += "+";
				}
				reasons += decision.hits[i].name;
			}

			const char* name = decision.name.empty() ? "<unnamed>" : decision.name.c_str();
			if (decision.boss) {
				SKSE::log::info("boss +{} [{:08X}] base={:08X} {} ({}): {}", decision.score, formID, decision.baseID, name,
					decision.edid.empty() ? "-" : decision.edid.c_str(), reasons);
			} else if (debug) {
				SKSE::log::info("skip {} [{:08X}] base={:08X} {}: score {} < {} ({})", name, formID, decision.baseID,
					decision.edid.empty() ? "-" : decision.edid.c_str(), decision.score,
					MakeSnapshot().threshold, reasons.empty() ? "no signal" : reasons.c_str());
			}
		}

		if (!decision.boss) {
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
