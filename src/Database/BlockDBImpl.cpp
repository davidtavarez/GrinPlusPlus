#include "BlockDBImpl.h"

#include <Database/DatabaseException.h>
#include <Infrastructure/Logger.h>
#include <Common/Util/StringUtil.h>
#include <utility>
#include <string>
#include <filesystem.h>

BlockDB::BlockDB(
	const Config& config,
	DB* pDatabase,
	OptimisticTransactionDB* pTransactionDB,
	ColumnFamilyHandle* pDefaultHandle,
	ColumnFamilyHandle* pBlockHandle,
	ColumnFamilyHandle* pHeaderHandle,
	ColumnFamilyHandle* pBlockSumsHandle,
	ColumnFamilyHandle* pOutputPosHandle,
	ColumnFamilyHandle* pInputBitmapHandle,
	ColumnFamilyHandle* pSpentOutputsHandle)
	: m_config(config),
	m_pDatabase(pDatabase),
	m_pTransactionDB(pTransactionDB),
	m_pTransaction(nullptr),
	m_pDefaultHandle(pDefaultHandle),
	m_pBlockHandle(pBlockHandle),
	m_pHeaderHandle(pHeaderHandle),
	m_pBlockSumsHandle(pBlockSumsHandle),
	m_pOutputPosHandle(pOutputPosHandle),
	m_pInputBitmapHandle(pInputBitmapHandle),
	m_pSpentOutputsHandle(pSpentOutputsHandle),
	m_blockHeadersCache(128)
{

}

BlockDB::~BlockDB()
{
	delete m_pBlockHandle;
	delete m_pHeaderHandle;
	delete m_pBlockSumsHandle;
	delete m_pOutputPosHandle;
	delete m_pInputBitmapHandle;
	delete m_pSpentOutputsHandle;
	delete m_pTransactionDB;
}

std::shared_ptr<BlockDB> BlockDB::OpenDB(const Config& config)
{
	Options options;
	options.IncreaseParallelism();

	// create the DB if it's not already present
	options.create_if_missing = true;
	options.compression = kNoCompression;

	// open DB
	fs::path dbPath = config.GetNodeConfig().GetDatabasePath() / "CHAIN/";
	fs::create_directories(dbPath);

	ColumnFamilyDescriptor BLOCK_COLUMN = ColumnFamilyDescriptor("BLOCK", *ColumnFamilyOptions().OptimizeForPointLookup(1024));
	ColumnFamilyDescriptor HEADER_COLUMN = ColumnFamilyDescriptor("HEADER", *ColumnFamilyOptions().OptimizeForPointLookup(1024));
	ColumnFamilyDescriptor BLOCK_SUMS_COLUMN = ColumnFamilyDescriptor("BLOCK_SUMS", *ColumnFamilyOptions().OptimizeForPointLookup(1024));
	ColumnFamilyDescriptor OUTPUT_POS_COLUMN = ColumnFamilyDescriptor("OUTPUT_POS", *ColumnFamilyOptions().OptimizeForPointLookup(1024));
	ColumnFamilyDescriptor INPUT_BITMAP_COLUMN = ColumnFamilyDescriptor("INPUT_BITMAP", *ColumnFamilyOptions().OptimizeForPointLookup(1024));
	ColumnFamilyDescriptor SPENT_OUTPUTS_COLUMN = ColumnFamilyDescriptor("SPENT_OUTPUTS", *ColumnFamilyOptions().OptimizeForPointLookup(1024));

	std::vector<std::string> columnFamilies;
	Status status = OptimisticTransactionDB::ListColumnFamilies(options, dbPath.u8string(), &columnFamilies);

	OptimisticTransactionDB* pTransactionDB = nullptr;
	DB* pDatabase = nullptr;
	ColumnFamilyHandle* pDefaultHandle = nullptr;
	ColumnFamilyHandle* pBlockHandle = nullptr;
	ColumnFamilyHandle* pHeaderHandle = nullptr;
	ColumnFamilyHandle* pBlockSumsHandle = nullptr;
	ColumnFamilyHandle* pOutputPosHandle = nullptr;
	ColumnFamilyHandle* pInputBitmapHandle = nullptr;
	ColumnFamilyHandle* pSpentOutputsHandle = nullptr;

	if (status.ok())
	{
		std::vector<ColumnFamilyDescriptor> columnDescriptors({ ColumnFamilyDescriptor(), BLOCK_COLUMN, HEADER_COLUMN, BLOCK_SUMS_COLUMN, OUTPUT_POS_COLUMN, INPUT_BITMAP_COLUMN, SPENT_OUTPUTS_COLUMN });
		columnDescriptors.resize(columnFamilies.size());

		std::vector<ColumnFamilyHandle*> columnHandles;

		status = OptimisticTransactionDB::Open(options, dbPath.u8string(), columnDescriptors, &columnHandles, &pTransactionDB);
		if (!status.ok())
		{
			throw DATABASE_EXCEPTION("DB::Open failed with error: " + std::string(status.getState()));
		}

		pDatabase = pTransactionDB->GetBaseDB();

		pDefaultHandle = columnHandles[0];
		pBlockHandle = columnHandles[1];
		pHeaderHandle = columnHandles[2];
		pBlockSumsHandle = columnHandles[3];
		pOutputPosHandle = columnHandles[4];
		pInputBitmapHandle = columnHandles[5];

		if (columnHandles.size() >= 7)
		{
			pSpentOutputsHandle = columnHandles[6];
		}
		else
		{
			pTransactionDB->CreateColumnFamily(SPENT_OUTPUTS_COLUMN.options, SPENT_OUTPUTS_COLUMN.name, &pSpentOutputsHandle);
		}
	}
	else
	{
		LOG_INFO("BlockDB not found. Creating it now.");

		std::vector<ColumnFamilyDescriptor> columnDescriptors({ ColumnFamilyDescriptor() });
		std::vector<ColumnFamilyHandle*> columnHandles;
		status = OptimisticTransactionDB::Open(options, dbPath.u8string(), columnDescriptors, &columnHandles, &pTransactionDB);
		if (!status.ok())
		{
			throw DATABASE_EXCEPTION("DB::Open failed with error: " + std::string(status.getState()));
		}

		pDatabase = pTransactionDB->GetBaseDB();

		pDefaultHandle = columnHandles[0];
		pDatabase->CreateColumnFamily(BLOCK_COLUMN.options, BLOCK_COLUMN.name, &pBlockHandle);
		pDatabase->CreateColumnFamily(HEADER_COLUMN.options, HEADER_COLUMN.name, &pHeaderHandle);
		pDatabase->CreateColumnFamily(BLOCK_SUMS_COLUMN.options, BLOCK_SUMS_COLUMN.name, &pBlockSumsHandle);
		pDatabase->CreateColumnFamily(OUTPUT_POS_COLUMN.options, OUTPUT_POS_COLUMN.name, &pOutputPosHandle);
		pDatabase->CreateColumnFamily(INPUT_BITMAP_COLUMN.options, INPUT_BITMAP_COLUMN.name, &pInputBitmapHandle);
		pTransactionDB->CreateColumnFamily(SPENT_OUTPUTS_COLUMN.options, SPENT_OUTPUTS_COLUMN.name, &pSpentOutputsHandle);
	}

	return std::shared_ptr<BlockDB>(new BlockDB(
		config,
		pDatabase,
		pTransactionDB,
		pDefaultHandle,
		pBlockHandle,
		pHeaderHandle,
		pBlockSumsHandle,
		pOutputPosHandle,
		pInputBitmapHandle,
		pSpentOutputsHandle
	));
}

void BlockDB::Commit()
{
	const Status status = m_pTransaction->Commit();
	if (!status.ok())
	{
		LOG_ERROR_F("Transaction::Commit failed with error ({})", status.getState());
		throw DATABASE_EXCEPTION("Transaction::Commit Failed with error: " + std::string(status.getState()));
	}

	for (auto pHeader : m_uncommitted)
	{
		m_blockHeadersCache.Put(pHeader->GetHash(), pHeader);
	}

	m_uncommitted.clear();
}

void BlockDB::Rollback() noexcept
{
	m_uncommitted.clear();
	const Status status = m_pTransaction->Rollback();
	if (!status.ok())
	{
		LOG_ERROR_F("Transaction::Rollback failed with error ({})", status.getState());
	}
}

void BlockDB::OnInitWrite()
{
	m_pTransaction = m_pTransactionDB->BeginTransaction(WriteOptions());
}

void BlockDB::OnEndWrite()
{
	delete m_pTransaction;
	m_pTransaction = nullptr;
}

Status BlockDB::Read(ColumnFamilyHandle* pFamilyHandle, const Slice& key, std::string* pValue) const
{
	if (m_pTransaction != nullptr)
	{
		return m_pTransaction->Get(ReadOptions(), pFamilyHandle, key, pValue);
	}
	else
	{
		return m_pDatabase->Get(ReadOptions(), pFamilyHandle, key, pValue);
	}
}

Status BlockDB::Write(ColumnFamilyHandle* pFamilyHandle, const Slice& key, const Slice& value)
{
	if (m_pTransaction != nullptr)
	{
		return m_pTransaction->Put(pFamilyHandle, key, value);
	}
	else
	{
		return m_pDatabase->Put(WriteOptions(), pFamilyHandle, key, value);
	}
}

Status BlockDB::Delete(ColumnFamilyHandle* pFamilyHandle, const Slice& key)
{
	if (m_pTransaction != nullptr)
	{
		return m_pTransaction->Delete(pFamilyHandle, key);
	}
	else
	{
		return m_pDatabase->Delete(WriteOptions(), pFamilyHandle, key);
	}
}

BlockHeaderPtr BlockDB::GetBlockHeader(const Hash& hash) const
{
	try
	{
		if (m_blockHeadersCache.Cached(hash))
		{
			return m_blockHeadersCache.Get(hash);
		}

		Slice key((const char*)hash.data(), hash.size());

		std::string value;
		const Status status = Read(m_pHeaderHandle, key, &value);
		if (status.ok())
		{
			std::vector<unsigned char> data(value.data(), value.data() + value.size());
			ByteBuffer byteBuffer(std::move(data));
			return std::make_unique<const BlockHeader>(BlockHeader::Deserialize(byteBuffer));
		}
		else if (status.IsNotFound())
		{
			LOG_ERROR_F("Header not found for hash {}", hash);
			return nullptr;
		}
		else
		{
			LOG_ERROR_F("DB::Get failed for hash ({}) with error ({})", hash, status.getState());
			throw DATABASE_EXCEPTION("DB::Get Failed with error: " + std::string(status.getState()));
		}
	}
	catch (DatabaseException&)
	{
		throw;
	}
	catch (std::exception& e)
	{
		LOG_ERROR_F("Exception caught: {}", e.what());
		throw DATABASE_EXCEPTION(e.what());
	}
}

void BlockDB::AddBlockHeader(BlockHeaderPtr pBlockHeader)
{
	LOG_TRACE_F("Adding header {}", *pBlockHeader);

	try
	{
		const Hash& hash = pBlockHeader->GetHash();

		Serializer serializer;
		pBlockHeader->Serialize(serializer);

		Slice key((const char*)hash.data(), hash.size());
		Slice value((const char*)serializer.data(), serializer.size());
		const Status status = Write(m_pHeaderHandle, Slice(key), value);
		if (!status.ok())
		{
			LOG_ERROR_F("DB::Put failed for header ({}) with error ({})", hash, status.getState());
			throw DATABASE_EXCEPTION("DB::Put Failed with error: " + std::string(status.getState()));
		}
		
		if (m_pTransaction != nullptr)
		{
			m_uncommitted.push_back(pBlockHeader);
		}
		else
		{
			m_blockHeadersCache.Put(pBlockHeader->GetHash(), pBlockHeader);
		}
	}
	catch (DatabaseException&)
	{
		throw;
	}
	catch (std::exception& e)
	{
		LOG_ERROR_F("Exception caught: {}", e.what());
		throw DATABASE_EXCEPTION("Error occurred: " + std::string(e.what()));
	}
}

void BlockDB::AddBlockHeaders(const std::vector<BlockHeaderPtr>& blockHeaders)
{
	LOG_TRACE_F("Adding {} headers.", blockHeaders.size());

	try
	{
		Status status;
		for (auto pBlockHeader : blockHeaders)
		{
			const Hash& hash = pBlockHeader->GetHash();

			Serializer serializer;
			pBlockHeader->Serialize(serializer);

			Slice key((const char*)hash.data(), hash.size());
			Slice value((const char*)serializer.data(), serializer.size());

			status = m_pTransaction->Put(m_pHeaderHandle, key, value);
			if (!status.ok())
			{
				LOG_ERROR_F("WriteBatch::put failed for header ({}) with error ({})", *pBlockHeader, status.getState());
				throw DATABASE_EXCEPTION("WriteBatch::put failed with error: " + std::string(status.getState()));
			}
		}
	}
	catch (DatabaseException&)
	{
		throw;
	}
	catch (std::exception& e)
	{
		LOG_ERROR_F("Exception caught: {}", e.what());
		throw DATABASE_EXCEPTION("Error occurred: " + std::string(e.what()));
	}

	LOG_TRACE("Finished adding headers.");
}

void BlockDB::AddBlock(const FullBlock& block)
{
	LOG_TRACE("Adding block");
	const std::vector<unsigned char>& hash = block.GetHash().GetData();

	Serializer serializer;
	block.Serialize(serializer);

	Slice key((const char*)hash.data(), hash.size());
	Slice value((const char*)serializer.data(), serializer.size());
	const Status status = Write(m_pBlockHandle, Slice(key), value);
	if (!status.ok())
	{
		LOG_ERROR_F("Failed to save Block: {}", block);
		throw DATABASE_EXCEPTION("Failed to save Block.");
	}
}

std::unique_ptr<FullBlock> BlockDB::GetBlock(const Hash& hash) const
{
	std::unique_ptr<FullBlock> pBlock = std::unique_ptr<FullBlock>(nullptr);

	Slice key((const char*)hash.data(), hash.size());
	std::string value;
	Status s = Read(m_pBlockHandle, key, &value);
	if (s.ok())
	{
		std::vector<unsigned char> data(value.data(), value.data() + value.size());
		ByteBuffer byteBuffer(std::move(data));
		pBlock = std::make_unique<FullBlock>(FullBlock::Deserialize(byteBuffer));
	}

	return pBlock;
}

void BlockDB::AddBlockSums(const Hash& blockHash, const BlockSums& blockSums)
{
	LOG_TRACE_F("Adding BlockSums for block {}", blockHash);

	Slice key((const char*)blockHash.data(), blockHash.size());

	// Serializes the BlockSums object
	Serializer serializer;
	blockSums.Serialize(serializer);
	Slice value((const char*)serializer.data(), serializer.size());

	// Insert BlockSums object
	const Status status = Write(m_pBlockSumsHandle, key, value);
	if (!status.ok())
	{
		LOG_ERROR_F("Failed to save BlockSums for {}", blockHash);
		throw DATABASE_EXCEPTION("Failed to save BlockSums.");
	}
}

std::unique_ptr<BlockSums> BlockDB::GetBlockSums(const Hash& blockHash) const
{
	std::unique_ptr<BlockSums> pBlockSums = std::unique_ptr<BlockSums>(nullptr);

	// Read from DB
	Slice key((const char*)blockHash.data(), blockHash.size());
	std::string value;
	const Status s = Read(m_pBlockSumsHandle, key, &value);
	if (s.ok())
	{
		// Deserialize result
		std::vector<unsigned char> data(value.data(), value.data() + value.size());
		ByteBuffer byteBuffer(std::move(data));
		pBlockSums = std::make_unique<BlockSums>(BlockSums::Deserialize(byteBuffer));
	}

	return pBlockSums;
}

void BlockDB::AddOutputPosition(const Commitment& outputCommitment, const OutputLocation& location)
{
	Slice key((const char*)outputCommitment.data(), 32);

	// Serializes the output position
	Serializer serializer;
	location.Serialize(serializer);
	Slice value((const char*)serializer.data(), serializer.size());

	// Insert the output position
	const Status status = Write(m_pOutputPosHandle, key, value);
	if (!status.ok())
	{
		LOG_ERROR_F("Failed to save location for output {}", outputCommitment);
		throw DATABASE_EXCEPTION("Failed to save output location.");
	}
}

std::unique_ptr<OutputLocation> BlockDB::GetOutputPosition(const Commitment& outputCommitment) const
{
	std::unique_ptr<OutputLocation> pOutputPosition = nullptr;

	Slice key((const char*)outputCommitment.data(), 32);

	// Read from DB
	std::string value;
	const Status s = Read(m_pOutputPosHandle, key, &value);
	if (s.ok())
	{
		// Deserialize result
		std::vector<unsigned char> data(value.data(), value.data() + value.size());
		ByteBuffer byteBuffer(std::move(data));
		pOutputPosition = std::make_unique<OutputLocation>(OutputLocation::Deserialize(byteBuffer));
	}

	return pOutputPosition;
}

void BlockDB::RemoveOutputPositions(const std::vector<Commitment>& outputCommitments)
{
	try
	{
		for (const Commitment& commit : outputCommitments)
		{
			const Status s = Delete(m_pOutputPosHandle, Slice((const char*)commit.data(), 32));
			if (!s.ok())
			{
				LOG_ERROR_F("Failed to delete location for output {}", commit);
				throw DATABASE_EXCEPTION("Failed to delete output location.");
			}
		}
	}
	catch (DatabaseException&)
	{
		throw;
	}
	catch (std::exception& e)
	{
		LOG_ERROR_F("Exception caught: {}", e.what());
		throw DATABASE_EXCEPTION(e.what());
	}
}

void BlockDB::AddBlockInputBitmap(const Hash& blockHash, const Roaring& bitmap)
{
	try
	{
		Slice key((const char*)blockHash.data(), blockHash.size());

		// Serializes the bitmap
		const size_t bitmapSize = bitmap.getSizeInBytes();
		std::vector<char> serializedBitmap(bitmapSize);
		const size_t bytesWritten = bitmap.write(serializedBitmap.data(), true);
		if (bytesWritten != bitmapSize)
		{
			LOG_ERROR_F("Expected to write {} bytes but wrote {}", bitmapSize, bytesWritten);
			throw DATABASE_EXCEPTION("Roaring bitmap did not serialize to expected number of bytes.");
		}

		Slice value(serializedBitmap.data(), bitmapSize);

		// Insert the output position
		const Status status = Write(m_pInputBitmapHandle, key, value);
	}
	catch (DatabaseException&)
	{
		throw;
	}
	catch (std::exception& e)
	{
		LOG_ERROR_F("Exception caught: {}", e.what());
		throw DATABASE_EXCEPTION(e.what());
	}
}

std::unique_ptr<Roaring> BlockDB::GetBlockInputBitmap(const Hash& blockHash) const
{
	try
	{
		Slice key((const char*)blockHash.data(), blockHash.size());

		// Read from DB
		std::string value;
		const Status s = Read(m_pInputBitmapHandle, key, &value);
		if (s.ok())
		{
			// Deserialize result
			return std::make_unique<Roaring>(Roaring::readSafe(value.data(), value.size()));
		}
		else if (s.IsNotFound())
		{
			LOG_ERROR_F("Block input bitmap not found for block {}", blockHash);
			return nullptr;
		}
		else
		{
			LOG_ERROR_F("DB::Get failed for block ({}) with error ({})", blockHash, s.getState());
			throw DATABASE_EXCEPTION("DB::Get Failed with error: " + std::string(s.getState()));
		}
	}
	catch (DatabaseException&)
	{
		throw;
	}
	catch (std::exception& e)
	{
		LOG_ERROR_F("Exception caught: {}", e.what());
		throw DATABASE_EXCEPTION(e.what());
	}
}

void BlockDB::AddSpentPositions(const Hash& blockHash, const std::vector<SpentOutput>& outputPositions)
{
	assert(outputPositions.size() < (size_t)UINT16_MAX);

	try
	{
		Slice key((const char*)blockHash.data(), blockHash.size());
		
		Serializer serializer;
		serializer.Append<uint8_t>(/* version= */ 0);
		serializer.Append<uint16_t>((uint16_t)outputPositions.size());
		
		for (const SpentOutput& output : outputPositions)
		{
			output.Serialize(serializer);
		}

		Slice value((const char*)serializer.data(), serializer.size());

		// Insert the spent output positions
		const Status s = Write(m_pSpentOutputsHandle, key, value);
		if (!s.ok())
		{
			LOG_ERROR_F("Failed to write spent outputs for block {}", blockHash);
			throw DATABASE_EXCEPTION("Failed to write spent outputs.");
		}
	}
	catch (DatabaseException&)
	{
		throw;
	}
	catch (std::exception & e)
	{
		LOG_ERROR_F("Exception caught: {}", e.what());
		throw DATABASE_EXCEPTION(e.what());
	}
}

std::unordered_map<Commitment, OutputLocation> BlockDB::GetSpentPositions(const Hash& blockHash) const
{
	try
	{
		Slice key((const char*)blockHash.data(), blockHash.size());

		// Read from DB
		std::string value;
		const Status s = Read(m_pSpentOutputsHandle, key, &value);
		if (s.ok())
		{
			// Deserialize result
			std::vector<unsigned char> bytes(value.data(), value.data() + value.size());
			ByteBuffer byteBuffer(bytes);
			const uint8_t version = byteBuffer.ReadU8();
			const uint16_t numPositions = byteBuffer.ReadU16();

			std::unordered_map<Commitment, OutputLocation> spentOutputs;
			spentOutputs.reserve(numPositions);

			for (uint16_t i = 0; i < numPositions; i++)
			{
				SpentOutput spent = SpentOutput::Deserialize(byteBuffer);
				spentOutputs.insert({ spent.GetCommitment(), spent.GetLocation() });
			}

			return spentOutputs;
		}
		else
		{
			LOG_ERROR_F("Failed to retrieve spent positions for block ({}) with error ({})", blockHash, s.getState());
			throw DATABASE_EXCEPTION("Failed to retrieve spent positions with error: " + std::string(s.getState()));
		}
	}
	catch (DatabaseException&)
	{
		throw;
	}
	catch (std::exception & e)
	{
		LOG_ERROR_F("Exception caught: {}", e.what());
		throw DATABASE_EXCEPTION(e.what());
	}
}