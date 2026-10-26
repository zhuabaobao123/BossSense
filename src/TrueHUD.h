#pragma once

#include <RE/Skyrim.h>

namespace bosssense::truehud
{
	/// Called at kPostPostLoad, once every SKSE plugin is loaded.
	bool Init();
	bool Available();

	void AddBoss(RE::Actor* a_actor);
	void RemoveBoss(RE::Actor* a_actor);
}
