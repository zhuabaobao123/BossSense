#include "PluginScanner.h"

#include <SKSE/SKSE.h>
#include <miniz.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace bosssense::scanner
{
	namespace
	{
		std::unordered_map<RE::FormID, NpcInfo> g_db;

		struct Stats
		{
			std::uint32_t files = 0;
			std::uint32_t npcRecords = 0;
			std::uint32_t stored = 0;
			std::uint32_t compressed = 0;
			std::uint32_t lookupMiss = 0;
		};

#pragma pack(push, 1)
		struct RecordHeader
		{
			char          type[4];
			std::uint32_t dataSize;
			std::uint32_t flags;
			std::uint32_t formID;
			std::uint16_t timestamp;
			std::uint16_t versionControl;
			std::uint16_t internalVersion;
			std::uint16_t unknown;
		};

		struct GroupHeader
		{
			char          type[4];
			std::uint32_t size;
			char          label[4];
			std::int32_t  groupType;
			std::uint16_t stamp;
			std::uint16_t unknown1;
			std::uint16_t version;
			std::uint16_t unknown2;
		};
#pragma pack(pop)

		static_assert(sizeof(RecordHeader) == 24);
		static_assert(sizeof(GroupHeader) == 24);

		constexpr std::uint32_t kCompressedFlag = 0x00040000;

		bool ContainsBoss(std::string_view a_text)
		{
			if (a_text.size() < 4) {
				return false;
			}
			std::string lower{ a_text };
			std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return lower.find("boss") != std::string::npos;
		}

		/// Hand-authored NPC records have a real EditorID; generated templates start
		/// with Lvl (leveled actor) or Enc (generic encounter actor).
		bool IsCustomName(std::string_view a_editorID)
		{
			if (a_editorID.empty()) {
				return false;
			}
			if (a_editorID.size() >= 3) {
				std::string prefix{ a_editorID.substr(0, 3) };
				std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (prefix == "lvl" || prefix == "enc") {
					return false;
				}
			}
			return true;
		}

		struct ParsedNpc
		{
			std::string edid;
			bool        unique = false;
			bool        aidtBoss = false;
		};

		void ParseSubrecords(const char* a_data, std::size_t a_size, ParsedNpc& a_out)
		{
			std::size_t pos = 0;
			std::uint32_t pendingSize = 0;
			bool          hasPending = false;

			while (pos + 6 <= a_size) {
				char type[5]{};
				std::memcpy(type, a_data + pos, 4);
				std::uint16_t size = 0;
				std::memcpy(&size, a_data + pos + 4, sizeof(size));
				pos += 6;

				if (std::memcmp(type, "XXXX", 4) == 0) {
					if (pos + 4 > a_size) {
						break;
					}
					std::memcpy(&pendingSize, a_data + pos, sizeof(pendingSize));
					pos += 4;
					hasPending = true;
					continue;
				}

				const std::uint32_t realSize = hasPending ? pendingSize : size;
				hasPending = false;
				if (realSize > a_size || pos + realSize > a_size) {
					break;
				}

				if (std::memcmp(type, "EDID", 4) == 0) {
					const auto len = strnlen(a_data + pos, realSize);
					a_out.edid.assign(a_data + pos, len);
				} else if (std::memcmp(type, "ACBS", 4) == 0 && realSize >= 4) {
					std::uint32_t flags = 0;
					std::memcpy(&flags, a_data + pos, sizeof(flags));
					a_out.unique = (flags & 0x20u) != 0;  // TESActorBaseData::Flag::kUnique
				} else if (std::memcmp(type, "AIDT", 4) == 0 && realSize >= 2) {
					const auto aggression = static_cast<std::int8_t>(a_data[pos]);
					const auto confidence = static_cast<std::int8_t>(a_data[pos + 1]);
					a_out.aidtBoss = aggression >= 2 && confidence == 4;
				}

				pos += realSize;
			}
		}

		class FileReader
		{
		public:
			bool Open(const std::filesystem::path& a_path)
			{
				m_in.open(a_path, std::ios::binary);
				if (!m_in) {
					return false;
				}
				m_in.seekg(0, std::ios::end);
				m_size = static_cast<std::uint64_t>(m_in.tellg());
				m_in.seekg(0, std::ios::beg);
				m_pos = 0;
				return true;
			}

			[[nodiscard]] std::uint64_t Size() const { return m_size; }
			[[nodiscard]] std::uint64_t Tell() const { return m_pos; }
			[[nodiscard]] bool          Eof() const { return m_pos >= m_size; }

			bool Read(void* a_dst, std::size_t a_bytes)
			{
				if (m_pos + a_bytes > m_size) {
					return false;
				}
				m_in.read(static_cast<char*>(a_dst), static_cast<std::streamsize>(a_bytes));
				m_pos += a_bytes;
				return static_cast<bool>(m_in);
			}

			bool Skip(std::uint64_t a_bytes)
			{
				if (m_pos + a_bytes > m_size) {
					return false;
				}
				m_in.seekg(static_cast<std::streamoff>(a_bytes), std::ios::cur);
				m_pos += a_bytes;
				return static_cast<bool>(m_in);
			}

			bool Seek(std::uint64_t a_pos)
			{
				if (a_pos > m_size) {
					return false;
				}
				m_in.seekg(static_cast<std::streamoff>(a_pos), std::ios::beg);
				m_pos = a_pos;
				return static_cast<bool>(m_in);
			}

		private:
			std::ifstream m_in;
			std::uint64_t m_size = 0;
			std::uint64_t m_pos = 0;
		};

		void StoreRecord(RE::TESDataHandler* a_dataHandler, const RE::TESFile* a_file,
			const RecordHeader& a_header, const ParsedNpc& a_npc, Stats& a_stats)
		{
			const bool edidBoss = ContainsBoss(a_npc.edid);
			const bool customName = IsCustomName(a_npc.edid);

			const RE::FormID runtimeID = a_dataHandler->LookupFormID(a_header.formID, a_file->GetFilename());
			if (runtimeID == 0) {
				++a_stats.lookupMiss;
				return;
			}

			auto& entry = g_db[runtimeID];
			entry.edidBoss = entry.edidBoss || edidBoss;
			entry.unique = entry.unique || a_npc.unique;
			entry.aidtBoss = entry.aidtBoss || a_npc.aidtBoss;
			entry.customName = entry.customName || customName;
			if (entry.edid.empty()) {
				entry.edid = a_npc.edid;
			}
			++a_stats.stored;
		}

		void ReadRecord(FileReader& a_reader, const RecordHeader& a_header, ParsedNpc& a_out, Stats& a_stats)
		{
			if (a_header.dataSize == 0 || a_header.dataSize > 8u * 1024u * 1024u) {
				a_reader.Skip(a_header.dataSize);
				return;
			}

			std::vector<char> data(a_header.dataSize);
			if (!a_reader.Read(data.data(), data.size())) {
				return;
			}

			if ((a_header.flags & kCompressedFlag) != 0 && data.size() > 4) {
				std::uint32_t uncompressed = 0;
				std::memcpy(&uncompressed, data.data(), sizeof(uncompressed));
				if (uncompressed == 0 || uncompressed > 64u * 1024u * 1024u) {
					return;
				}
				std::vector<char> out(uncompressed);
				mz_ulong destLen = uncompressed;
				const auto rc = mz_uncompress(reinterpret_cast<unsigned char*>(out.data()), &destLen,
					reinterpret_cast<const unsigned char*>(data.data() + 4), static_cast<mz_ulong>(data.size() - 4));
				if (rc != MZ_OK) {
					return;
				}
				out.resize(destLen);
				++a_stats.compressed;
				ParseSubrecords(out.data(), out.size(), a_out);
			} else {
				ParseSubrecords(data.data(), data.size(), a_out);
			}
		}

		void ParseNpcGroup(FileReader& a_reader, std::uint64_t a_end, RE::TESDataHandler* a_dataHandler,
			const RE::TESFile* a_file, Stats& a_stats);

		void ParseGroup(FileReader& a_reader, std::uint64_t a_end, RE::TESDataHandler* a_dataHandler,
			const RE::TESFile* a_file, Stats& a_stats)
		{
			while (a_reader.Tell() + 8 <= a_end && !a_reader.Eof()) {
				const auto start = a_reader.Tell();
				char sig[4]{};
				std::uint32_t chunkSize = 0;
				if (!a_reader.Read(sig, sizeof(sig)) || !a_reader.Read(&chunkSize, sizeof(chunkSize))) {
					return;
				}

				if (std::memcmp(sig, "GRUP", 4) == 0) {
					GroupHeader group{};
					std::memcpy(&group.type, sig, 4);
					group.size = chunkSize;
					// GRUP: type[4] size[4] label[4] groupType[4] stamp[2] unknown[2] version[2] unknown[2]
					if (!a_reader.Read(reinterpret_cast<char*>(&group) + 8, 16)) {
						return;
					}
					const std::uint64_t groupEnd = start + group.size;
					if (groupEnd <= start || groupEnd > a_reader.Size()) {
						return;
					}
					if (std::memcmp(group.label, "NPC_", 4) == 0) {
						ParseNpcGroup(a_reader, groupEnd, a_dataHandler, a_file, a_stats);
					}
					a_reader.Seek(groupEnd);
				} else {
					RecordHeader header{};
					std::memcpy(&header.type, sig, 4);
					header.dataSize = chunkSize;
					// Record: type[4] size[4] flags[4] formID[4] timestamp[2] vc[2] internal[2] unknown[2]
					if (!a_reader.Read(reinterpret_cast<char*>(&header) + 8, 16)) {
						return;
					}
					const std::uint64_t recordEnd = start + sizeof(RecordHeader) + header.dataSize;
					if (recordEnd > a_reader.Size()) {
						return;
					}
					if (std::memcmp(header.type, "NPC_", 4) == 0) {
						++a_stats.npcRecords;
						ParsedNpc npc;
						ReadRecord(a_reader, header, npc, a_stats);
						StoreRecord(a_dataHandler, a_file, header, npc, a_stats);
					}
					a_reader.Seek(recordEnd);
				}
			}
		}

		void ParseNpcGroup(FileReader& a_reader, std::uint64_t a_end, RE::TESDataHandler* a_dataHandler,
			const RE::TESFile* a_file, Stats& a_stats)
		{
			ParseGroup(a_reader, a_end, a_dataHandler, a_file, a_stats);
		}

		void ScanFile(const RE::TESFile* a_file, Stats& a_stats)
		{
			if (!a_file) {
				return;
			}

			std::filesystem::path path = std::filesystem::path("Data") / std::string(a_file->GetFilename());
			std::error_code ec;
			if (!std::filesystem::exists(path, ec)) {
				path = std::filesystem::path(a_file->path);
				if (!std::filesystem::exists(path, ec)) {
					SKSE::log::warn("plugin not found on disk: {}", a_file->GetFilename());
					return;
				}
			}

			FileReader reader;
			if (!reader.Open(path)) {
				SKSE::log::warn("could not open {}", path.string());
				return;
			}

			RecordHeader tes4{};
			if (!reader.Read(&tes4, sizeof(tes4)) || std::memcmp(tes4.type, "TES4", 4) != 0) {
				SKSE::log::warn("not a TES4 plugin: {}", a_file->GetFilename());
				return;
			}
			if (!reader.Skip(tes4.dataSize)) {
				return;
			}

			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			while (reader.Tell() + 8 <= reader.Size() && !reader.Eof()) {
				const auto start = reader.Tell();
				char sig[4]{};
				std::uint32_t chunkSize = 0;
				if (!reader.Read(sig, sizeof(sig)) || !reader.Read(&chunkSize, sizeof(chunkSize))) {
					break;
				}

				if (std::memcmp(sig, "GRUP", 4) == 0) {
					GroupHeader group{};
					std::memcpy(&group.type, sig, 4);
					group.size = chunkSize;
					if (!reader.Read(reinterpret_cast<char*>(&group) + 8, 16)) {
						break;
					}
					const std::uint64_t groupEnd = start + group.size;
					if (groupEnd <= start || groupEnd > reader.Size()) {
						break;
					}
					if (std::memcmp(group.label, "NPC_", 4) == 0) {
						ParseNpcGroup(reader, groupEnd, dataHandler, a_file, a_stats);
					}
					reader.Seek(groupEnd);
				} else {
					RecordHeader header{};
					std::memcpy(&header.type, sig, 4);
					header.dataSize = chunkSize;
					if (!reader.Read(reinterpret_cast<char*>(&header) + 8, 16)) {
						break;
					}
					const std::uint64_t recordEnd = start + sizeof(RecordHeader) + header.dataSize;
					if (recordEnd > reader.Size()) {
						break;
					}
					reader.Seek(recordEnd);
				}
			}

			++a_stats.files;
		}
	}

	void ScanAll()
	{
		const auto startTime = std::chrono::steady_clock::now();
		g_db.clear();
		g_db.reserve(8192);

		Stats stats;
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			SKSE::log::error("no data handler, scan skipped");
			return;
		}

		auto* mods = dataHandler->GetLoadedMods();
		const auto modCount = dataHandler->GetLoadedModCount();
		for (std::uint32_t i = 0; i < modCount; ++i) {
			ScanFile(mods[i], stats);
		}

		auto* lightMods = dataHandler->GetLoadedLightMods();
		const auto lightCount = dataHandler->GetLoadedLightModCount();
		for (std::uint32_t i = 0; i < lightCount; ++i) {
			ScanFile(lightMods[i], stats);
		}

		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
		SKSE::log::info("scan: {} plugins, {} NPC_ records, {} stored, {} compressed, {} lookup misses, {} ms",
			stats.files, stats.npcRecords, stats.stored, stats.compressed, stats.lookupMiss, ms);
	}

	const NpcInfo* Find(RE::FormID a_formID)
	{
		if (a_formID == 0) {
			return nullptr;
		}
		const auto it = g_db.find(a_formID);
		return it != g_db.end() ? &it->second : nullptr;
	}

	std::size_t Count()
	{
		return g_db.size();
	}
}
