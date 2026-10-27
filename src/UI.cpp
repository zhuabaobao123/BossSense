#include "UI.h"

#include "BossManager.h"
#include "Config.h"
#include "Detector.h"
#include "PluginScanner.h"
#include "TrueHUD.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

#include <SKSEMenuFramework.h>

namespace bosssense::ui
{
	namespace
	{
		enum class PendingAction : int
		{
			kNone = 0,
			kTest,
			kBlacklist,
			kWhitelist,
			kRemoveFromLists,
			kReevaluate
		};

		std::atomic<int> g_pendingAction{ 0 };
		std::mutex       g_resultMutex;
		std::string      g_result;

		std::string FormEntry(RE::TESForm* a_form)
		{
			if (!a_form) {
				return {};
			}
			auto* file = a_form->GetFile(0);
			if (!file) {
				return {};
			}
			const auto id = a_form->GetFormID();
			const auto local = file->IsLight() ? (id & 0xFFF) : (id & 0xFFFFFF);

			char buffer[320]{};
			std::snprintf(buffer, sizeof(buffer), "%.*s:0x%06X",
				static_cast<int>(file->GetFilename().size()), file->GetFilename().data(), local);
			return buffer;
		}

		RE::FormID ResolveEntry(const std::string& a_entry)
		{
			const auto split = a_entry.find(':');
			if (split == std::string::npos) {
				return 0;
			}
			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler) {
				return 0;
			}
			RE::FormID local = 0;
			try {
				local = static_cast<RE::FormID>(std::stoul(a_entry.substr(split + 1), nullptr, 0));
			} catch (...) {
				return 0;
			}
			auto* form = dataHandler->LookupForm(local, a_entry.substr(0, split));
			return form ? form->GetFormID() : 0;
		}

		/// Dynamic templates (FFxxxxxx) have no plugin file, so fall back to the leveled
		/// original/template base, which is a real plugin form.
		RE::TESForm* PersistentBase(RE::Actor* a_actor)
		{
			auto* base = a_actor->GetActorBase();
			if (base && base->GetFile(0)) {
				return base;
			}
			if (auto* extra = static_cast<RE::ExtraLeveledCreature*>(
					a_actor->extraList.GetByType(RE::ExtraDataType::kLeveledCreature))) {
				if (extra->originalBase && extra->originalBase->GetFile(0)) {
					return extra->originalBase;
				}
				if (extra->templateBase && extra->templateBase->GetFile(0)) {
					return extra->templateBase;
				}
			}
			return base;
		}

		RE::Actor* CrosshairTarget()
		{
			auto* pick = RE::CrosshairPickData::GetSingleton();
			if (!pick) {
				return nullptr;
			}
			const auto handle = pick->GetActiveTarget();
			auto* ref = handle.get().get();
			return ref ? ref->As<RE::Actor>() : nullptr;
		}

		RE::Actor* CombatTarget()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return nullptr;
			}
			auto handle = player->GetActorRuntimeData().currentCombatTarget;
			return handle.get().get();
		}

		RE::Actor* CurrentTarget()
		{
			if (auto* actor = CrosshairTarget()) {
				return actor;
			}
			return CombatTarget();
		}

		std::string DescribeTarget(RE::Actor* a_target, bool a_boss, int a_score, int a_threshold, const std::string& a_edid, const Decision& a_decision)
		{
			std::string text = a_target->GetName() ? a_target->GetName() : "?";
			text += "  ";
			text += a_boss ? _T("=> BOSS") : _T("=> not boss");
			text += "  " + std::string(_T("score")) + " " + std::to_string(a_score) + "/" + std::to_string(a_threshold);
			if (!a_edid.empty()) {
				text += "  edid=" + a_edid;
			}
			text += "  [";
			for (int i = 0; i < a_decision.hitCount; ++i) {
				if (i) {
					text += "+";
				}
				text += a_decision.hits[i].name;
				text += "(";
				text += std::to_string(a_decision.hits[i].weight);
				text += ")";
			}
			text += "]";
			return text;
		}

		void RunPendingAction()
		{
			const auto action = static_cast<PendingAction>(g_pendingAction.exchange(0));
			if (action == PendingAction::kNone) {
				return;
			}

			auto* task = SKSE::GetTaskInterface();
			if (!task) {
				return;
			}

			task->AddTask([action]() {
				auto* target = CurrentTarget();
				std::string text;

				if (!target) {
					text = _T("No target under the crosshair or in combat.");
				} else if (action == PendingAction::kTest) {
					auto* player = RE::PlayerCharacter::GetSingleton();
					if (player && !target->IsHostileToActor(player)) {
						text = std::string(target->GetName() ? target->GetName() : "?") + "  " + _T("not hostile - ignored");
					} else {
						const auto decision = BossManager::Get().EvaluateOnly(target);
						text = DescribeTarget(target, decision.score >= decision.threshold, decision.score, decision.threshold, decision.edid, decision);
					}
				} else if (action == PendingAction::kReevaluate) {
					BossManager::Get().ReevaluateAll();
					BossManager::Get().ConsiderNow(target);
					const auto decision = BossManager::Get().EvaluateOnly(target);
					text = DescribeTarget(target, decision.score >= decision.threshold, decision.score, decision.threshold, decision.edid, decision);
				} else if (action == PendingAction::kRemoveFromLists) {
					std::vector<RE::FormID> ids;
					if (auto* base = target->GetActorBase()) {
						ids.push_back(base->GetFormID());
					}
					if (auto* extra = static_cast<RE::ExtraLeveledCreature*>(
							target->extraList.GetByType(RE::ExtraDataType::kLeveledCreature))) {
						if (extra->originalBase) {
							ids.push_back(extra->originalBase->GetFormID());
						}
						if (extra->templateBase) {
							ids.push_back(extra->templateBase->GetFormID());
						}
					}

					std::lock_guard lock(SettingsMutex());
					auto& cfg = MutableSettings();
					int removed = 0;
					for (const auto id : ids) {
						removed += static_cast<int>(cfg.blacklist.erase(id));
						removed += static_cast<int>(cfg.whitelist.erase(id));
					}
					auto eraseRaw = [&](std::vector<std::string>& a_raw) {
						std::erase_if(a_raw, [&](const std::string& entry) {
							const auto id = ResolveEntry(entry);
							return id != 0 && std::find(ids.begin(), ids.end(), id) != ids.end();
						});
					};
					eraseRaw(cfg.blacklistForms);
					eraseRaw(cfg.whitelistForms);
					cfg.dirty = true;

					text = removed > 0
						? std::string(_T("Removed from lists: ")) + std::to_string(removed)
						: _T("Target is not in either list.");
				} else {
					auto* form = PersistentBase(target);
					const auto entry = FormEntry(form);
					bool blacklisted = false;
					bool whitelisted = false;
					if (entry.empty()) {
						{
							std::lock_guard lock(SettingsMutex());
							auto& cfg = MutableSettings();
							if (form) {
								if (action == PendingAction::kBlacklist) {
									cfg.blacklist.insert(form->GetFormID());
									blacklisted = true;
								} else {
									cfg.whitelist.insert(form->GetFormID());
									whitelisted = true;
								}
							}
							cfg.dirty = true;
						}
						text = _T("Dynamic template - this override lasts only until the game restarts.");
					} else {
						{
							std::lock_guard lock(SettingsMutex());
							auto& cfg = MutableSettings();
							if (action == PendingAction::kBlacklist) {
								cfg.blacklistForms.push_back(entry);
								cfg.blacklist.insert(form->GetFormID());
								blacklisted = true;
								text = std::string(_T("Blacklisted: ")) + entry;
							} else {
								cfg.whitelistForms.push_back(entry);
								cfg.whitelist.insert(form->GetFormID());
								whitelisted = true;
								text = std::string(_T("Whitelisted: ")) + entry;
							}
							cfg.dirty = true;
						}
					}

					// Apply immediately: a blacklisted actor loses its bar now, a whitelisted one gets it now.
					if (blacklisted) {
						BossManager::Get().RemoveTracked(target);
					} else if (whitelisted) {
						BossManager::Get().ConsiderNow(target);
					}
				}

				std::lock_guard lock(g_resultMutex);
				g_result = text;
			});
		}

		bool SignalRow(const char* a_label, Signal* a_signal)
		{
			bool changed = false;
			ImGui::PushID(a_label);
			changed |= ImGui::Checkbox(a_label, &a_signal->enabled);
			if (a_signal->enabled) {
				ImGui::SameLine(0.0f, 12.0f);
				ImGui::SetNextItemWidth(240.0f);
				changed |= ImGui::SliderInt("##weight", &a_signal->weight, 0, 150, _T("+%d pts"));
			}
			ImGui::PopID();
			return changed;
		}

		void __stdcall Render()
		{
			Settings s;
			{
				std::lock_guard lock(SettingsMutex());
				s = MutableSettings();
			}

			RunPendingAction();

			bool changed = false;
			static bool needReeval = false;
			static auto lastChange = std::chrono::steady_clock::now();
			static auto lastSave = std::chrono::steady_clock::now();

			changed |= ImGui::Checkbox(_T("Enable dynamic boss detection"), &s.enabled);

			ImGui::TextUnformatted(_T("Boss = one strong signal, or enough small signals to reach the threshold."));
			changed |= ImGui::SliderInt(_T("Threshold"), &s.threshold, 10, 250);

			if (ImGui::CollapsingHeader(_T("Signals"), ImGuiTreeNodeFlags_DefaultOpen)) {
				changed |= SignalRow(_T("Race whitelist"), &s.race);
				changed |= SignalRow(_T("Location boss marker"), &s.locRef);
				changed |= SignalRow(_T("EditorID contains 'boss'"), &s.edid);
				changed |= SignalRow(_T("Hand-authored NPC"), &s.custom);
				changed |= SignalRow(_T("Not from a leveled list"), &s.notLeveled);
				changed |= SignalRow(_T("Unique flag"), &s.unique);
				changed |= SignalRow(_T("Level above player"), &s.level);
				changed |= SignalRow(_T("High absolute level"), &s.highLevel);
				changed |= SignalRow(_T("Large health pool"), &s.health);
				changed |= SignalRow(_T("Aggressive + foolhardy (AIDT)"), &s.aidt);
				changed |= SignalRow(_T("Has a name"), &s.named);

				changed |= ImGui::SliderInt(_T("Level delta"), &s.levelDelta, 0, 40);
				changed |= ImGui::SliderInt(_T("Min level"), &s.levelMin, 0, 150);
				changed |= ImGui::SliderFloat(_T("Min max-health"), &s.healthMin, 50.0f, 3000.0f, "%.0f");
			}

			if (ImGui::CollapsingHeader(_T("Behaviour"))) {
				changed |= ImGui::Checkbox(_T("Hostile targets only"), &s.onlyHostile);
				changed |= ImGui::Checkbox(_T("Only while in combat"), &s.requireCombat);
				changed |= ImGui::Checkbox(_T("Write debug log"), &s.debugLog);
			}

			ImGui::Separator();
			ImGui::TextUnformatted(_T("Target (crosshair first)"));

			if (ImGui::Button(_T("Evaluate target"))) {
				g_pendingAction = static_cast<int>(PendingAction::kTest);
			}
			ImGui::SameLine();
			if (ImGui::Button(_T("Re-evaluate target"))) {
				g_pendingAction = static_cast<int>(PendingAction::kReevaluate);
			}
			ImGui::SameLine();
			if (ImGui::Button(_T("Never boss"))) {
				g_pendingAction = static_cast<int>(PendingAction::kBlacklist);
			}
			ImGui::SameLine();
			if (ImGui::Button(_T("Always boss"))) {
				g_pendingAction = static_cast<int>(PendingAction::kWhitelist);
			}
			ImGui::SameLine();
			if (ImGui::Button(_T("Remove from lists"))) {
				g_pendingAction = static_cast<int>(PendingAction::kRemoveFromLists);
			}

			{
				std::lock_guard lock(g_resultMutex);
				if (!g_result.empty()) {
					ImGui::TextWrapped("%s", g_result.c_str());
				}
			}

			ImGui::Separator();
			ImGui::Text(_T("Boss bars added this session: %d"), BossManager::Get().SessionCount());
			const auto last = BossManager::Get().LastSummary();
			if (!last.empty()) {
				ImGui::Text(_T("Last boss: %s"), last.c_str());
			}
			ImGui::Text(_T("TrueHUD API: %s"), truehud::Available() ? "OK" : _T("missing"));
			ImGui::Text(_T("Indexed NPC records: %d"), static_cast<int>(scanner::Count()));

			ImGui::Separator();
			if (ImGui::Button(_T("Save settings now"))) {
				{
					std::lock_guard lock(SettingsMutex());
					MutableSettings() = s;
					MutableSettings().dirty = true;
				}
				SaveSettings();
				std::lock_guard lock(g_resultMutex);
				g_result = _T("Settings saved.");
			}

			const auto now = std::chrono::steady_clock::now();

			if (changed) {
				{
					std::lock_guard lock(SettingsMutex());
					auto& global = MutableSettings();
					global = s;
					global.dirty = true;
				}
				lastChange = now;
				needReeval = true;

				// Refresh the displayed score while a slider is being dragged.
				static auto lastTest = std::chrono::steady_clock::now();
				if (now - lastTest > std::chrono::milliseconds(100)) {
					g_pendingAction = static_cast<int>(PendingAction::kTest);
					lastTest = now;
				}
			}

			// Settings changed: clear the per-combat decision cache and re-check the target.
			if (needReeval && now - lastChange > std::chrono::milliseconds(700)) {
				g_pendingAction = static_cast<int>(PendingAction::kReevaluate);
				needReeval = false;
			}

			if (now - lastSave > std::chrono::milliseconds(1500)) {
				bool dirty = false;
				{
					std::lock_guard lock(SettingsMutex());
					dirty = MutableSettings().dirty;
				}
				if (dirty) {
					SaveSettings();
					lastSave = now;
				}
			}
		}
	}

	void Register()
	{
		// The framework DLL may load after this plugin, so refresh the header's cached handle.
		menuFramework = GetModuleHandleW(L"SKSEMenuFramework");

		if (!SKSEMenuFramework::IsInstalled() || !menuFramework) {
			SKSE::log::info("SKSE Menu Framework not installed; settings menu disabled");
			return;
		}

		SKSEMenuFramework::SetSection(_T("Boss Sense"));
		SKSEMenuFramework::AddSectionItem(_T("Settings"), Render);

		SKSE::log::info("SKSE Menu Framework page registered");
	}
}
