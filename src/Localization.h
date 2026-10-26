#pragma once

namespace Localization
{
	/// Load <DLL>.json from the plugin's own folder. Safe to call once, before any UI text is used.
	void Load();

	/// Returns the translated text, or the original English key when the entry is missing.
	[[nodiscard]] const char* Translate(const char* a_key);
}
