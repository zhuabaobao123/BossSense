#pragma once

#include <RE/Skyrim.h>

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_set>

namespace bosssense
{
	struct Decision;

	class BossManager final :
		public RE::BSTEventSink<RE::TESCombatEvent>,
		public RE::BSTEventSink<RE::TESDeathEvent>,
		public RE::BSTEventSink<RE::TESHitEvent>
	{
	public:
		static BossManager& Get();

		void Register();
		void Reset();

		/// Clear the per-combat decision cache so the next evaluation runs again.
		void Invalidate();
		/// Evaluate and add a boss bar immediately (used by the menu).
		void ConsiderNow(RE::Actor* a_actor);

		// Test hook for the menu: evaluate without adding.
		Decision EvaluateOnly(RE::Actor* a_actor);

		std::string LastSummary();
		int         SessionCount() const { return m_sessionCount.load(); }

		RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>* a_source) override;
		RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>* a_source) override;
		RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>* a_source) override;

	private:
		BossManager() = default;

		void Consider(RE::Actor* a_actor);
		void Forget(RE::FormID a_formID);

		std::unordered_set<RE::FormID> m_added;
		std::unordered_set<RE::FormID> m_decided;
		std::atomic<int>               m_sessionCount{ 0 };

		std::mutex  m_summaryMutex;
		std::string m_lastSummary;
	};
}
