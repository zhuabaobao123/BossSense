#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace bosssense::scanner
{
	struct NpcInfo
	{
		bool        edidBoss = false;
		bool        unique = false;
		bool        aidtBoss = false;
		bool        customName = false;
		std::string edid;
	};

	/// Walk the active plugins' NPC_ groups once, at kDataLoaded.
	void ScanAll();

	const NpcInfo* Find(RE::FormID a_formID);

	std::size_t Count();
}
