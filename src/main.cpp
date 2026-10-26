#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <spdlog/sinks/basic_file_sink.h>

#include "BossManager.h"
#include "Config.h"
#include "Detector.h"
#include "Localization.h"
#include "PluginScanner.h"
#include "TrueHUD.h"
#include "UI.h"

namespace
{
	void SetupLog()
	{
		auto path = SKSE::log::log_directory();
		if (!path) {
			return;
		}

		*path /= "BossSense.log";

		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
		auto log = std::make_shared<spdlog::logger>("BossSense", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) {
			return;
		}

		switch (a_msg->type) {
		case SKSE::MessagingInterface::kPostPostLoad:
			SKSE::log::info("post-post-load: requesting TrueHUD API");
			bosssense::truehud::Init();
			break;

		case SKSE::MessagingInterface::kDataLoaded:
			SKSE::log::info("data loaded: loading settings and scanning plugins");
			bosssense::LoadSettings();
			bosssense::ResolveFormLists();
			bosssense::InitDetectorForms();
			bosssense::scanner::ScanAll();
			bosssense::BossManager::Get().Register();
			Localization::Load();
			bosssense::ui::Register();
			SKSE::log::info("ready: {} NPC records indexed", bosssense::scanner::Count());
			break;

		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			bosssense::BossManager::Get().Reset();
			break;

		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SetupLog();
	SKSE::Init(a_skse, SKSE::InitInfo{ .log = false });

	SKSE::log::info("BossSense 1.0.0 loading");

	auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging) {
		SKSE::log::critical("no messaging interface");
		return false;
	}
	if (!messaging->RegisterListener(OnMessage)) {
		SKSE::log::critical("could not register messaging listener");
		return false;
	}

	return true;
}
