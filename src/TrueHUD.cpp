#include "TrueHUD.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string_view>

#include <windows.h>

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <TrueHUDAPI.h>

namespace bosssense::truehud
{
	namespace
	{
		TRUEHUD_API::IVTrueHUD4* g_api = nullptr;
	}

	bool Init()
	{
		if (g_api) {
			return true;
		}

		void* api = TRUEHUD_API::RequestPluginAPI(TRUEHUD_API::InterfaceVersion::V4);
		if (!api) {
			api = TRUEHUD_API::RequestPluginAPI(TRUEHUD_API::InterfaceVersion::V3);
		}
		if (!api) {
			SKSE::log::warn("TrueHUD not found; boss bars disabled");
			return false;
		}

		g_api = static_cast<TRUEHUD_API::IVTrueHUD4*>(api);
		SKSE::log::info("TrueHUD API acquired (thread {})", g_api->GetTrueHUDThreadId());
		return true;
	}

	bool Available()
	{
		return g_api != nullptr;
	}

	void AddBoss(RE::Actor* a_actor)
	{
		if (g_api && a_actor) {
			g_api->AddBoss(a_actor->GetHandle());
		}
	}

	void RemoveBoss(RE::Actor* a_actor)
	{
		if (g_api && a_actor) {
			g_api->RemoveBoss(a_actor->GetHandle(), TRUEHUD_API::WidgetRemovalMode::Normal);
		}
	}
}
