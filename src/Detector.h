#pragma once

#include <RE/Skyrim.h>

#include <array>
#include <string>

namespace bosssense
{
	struct Decision
	{
		struct Hit
		{
			const char* name;
			int         weight;
		};

		bool        boss = false;
		int         score = 0;
		std::array<Hit, 10> hits{};
		int         hitCount = 0;

		RE::FormID  baseID = 0;
		std::string name;
		std::string edid;

		bool Whitelisted = false;
		bool Blacklisted = false;
	};

	/// Resolve the two vanilla location-ref-type forms used as boss markers.
	void InitDetectorForms();

	Decision Evaluate(RE::Actor* a_actor);
}
