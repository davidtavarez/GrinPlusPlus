#pragma once

#include "PruneList.h"
#include "MMRUtil.h"
#include "MMRHashUtil.h"

#include <string>
#include <Crypto/Hash.h>
#include <Roaring.h>
#include <filesystem.h>
#include <Core/File/BitmapFile.h>
#include <Common/Util/HexUtil.h>
#include <Common/Util/FileUtil.h>
#include <Core/Serialization/Serializer.h>

#include <filesystem.h>

class LeafSet
{
public:
	static std::shared_ptr<LeafSet> Load(const fs::path& path)
	{
		auto pBitmapFile = BitmapFile::Load(path);

		return std::shared_ptr<LeafSet>(new LeafSet(path, pBitmapFile));
	}

	void Add(const uint64_t leafIndex) { m_pBitmap->Set(leafIndex); }
	void Remove(const uint64_t leafIndex) { m_pBitmap->Unset(leafIndex); }
	bool Contains(const uint64_t leafIndex) const { return m_pBitmap->IsSet(leafIndex); }

	void Rewind(const uint64_t numLeaves, const std::vector<uint64_t>& leavesToAdd) { m_pBitmap->Rewind(numLeaves, leavesToAdd); }
	void Commit() { m_pBitmap->Commit(); }
	void Rollback() noexcept { m_pBitmap->Rollback(); }
	void Snapshot(const Hash& blockHash)
	{
		std::string path = m_path.u8string() + "." + HASH::ShortHash(blockHash);
		Roaring snapshotBitmap = m_pBitmap->ToRoaring();

		const size_t numBytes = snapshotBitmap.getSizeInBytes();
		std::vector<unsigned char> bytes(numBytes);
		const size_t bytesWritten = snapshotBitmap.write((char*)bytes.data());
		if (bytesWritten != numBytes)
		{
			throw std::exception(); // TODO: Handle this.
		}

		FileUtil::SafeWriteToFile(FileUtil::ToPath(path), bytes);
	}

	Hash Root(const uint64_t numOutputs) const
	{
		const fs::path path = fs::temp_directory_path() / "UBMT";
		std::shared_ptr<HashFile> pHashFile = HashFile::Load(path);
		pHashFile->Rewind(0);

		size_t index = 0;
		std::vector<uint8_t> bytes(128);
		const uint64_t numChunks = (numOutputs + 1023) / 1024;
		for (size_t i = 0; i < numChunks; i++)
		{
			for (size_t j = 0; j < 128; j++)
			{
				bytes[j] = m_pBitmap->GetByte(index++);
			}

			MMRHashUtil::AddHashes(pHashFile, bytes, nullptr);
		}

		return MMRHashUtil::Root(pHashFile, pHashFile->GetSize(), nullptr);
	}

private:
	LeafSet(const fs::path& path, std::shared_ptr<BitmapFile> pBitmap)
		: m_path(path), m_pBitmap(pBitmap)
	{

	}

	fs::path m_path;
	std::shared_ptr<BitmapFile> m_pBitmap;
};
