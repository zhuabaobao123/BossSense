#pragma once

#include <RE/Skyrim.h>

namespace bosssense::distribution
{
	/// Resolve the [Distribution] entries from BossSense.ini. Call once, after kDataLoaded.
	void Init();

	/// Hand the configured item/spell/perk to an actor that was just accepted as a boss.
	/// Presence-checked, so a second call (or a fight reloaded from a save) never stacks copies.
	void Give(RE::Actor* a_actor, bool a_items, bool a_spells, bool a_perks);
}
