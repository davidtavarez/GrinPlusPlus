#include "SqliteStore.h"
#include "WalletSqlite.h"
#include "Tables/VersionTable.h"
#include "Tables/OutputsTable.h"
#include "Tables/TransactionsTable.h"
#include "Tables/MetadataTable.h"

#include <Wallet/WalletDB/WalletStoreException.h>
#include <Common/Util/FileUtil.h>
#include <Common/Util/StringUtil.h>
#include <Infrastructure/Logger.h>

static const uint8_t ENCRYPTION_FORMAT = 0;
static const int LATEST_SCHEMA_VERSION = 1;

std::shared_ptr<SqliteStore> SqliteStore::Open(const Config& config)
{
	return std::make_shared<SqliteStore>(SqliteStore(config.GetWalletConfig().GetWalletDirectory()));
}

fs::path SqliteStore::GetDBFile(const std::string& username) const
{
	return m_walletDirectory / StringUtil::ToWide(username) / "wallet.db";
}

std::vector<std::string> SqliteStore::GetAccounts() const
{
	return FileUtil::GetSubDirectories(m_walletDirectory, false);
}

Locked<IWalletDB> SqliteStore::OpenWallet(const std::string& username, const SecureVector& masterSeed)
{
	auto iter = m_userDBs.find(username);
	if (iter != m_userDBs.end())
	{
		WALLET_DEBUG_F("wallet.db already open for user: {}", username);
		return iter->second;
	}

	const fs::path dbFile = GetDBFile(username);
	if (!FileUtil::Exists(dbFile))
	{
		throw WALLET_STORE_EXCEPTION("Wallet does not exist.");
	}

	sqlite3* pDatabase = nullptr;

	try
	{
		if (sqlite3_open(dbFile.u8string().c_str(), &pDatabase) != SQLITE_OK)
		{
			WALLET_ERROR_F("Failed to open wallet.db for user ({}) with error ({})", username, sqlite3_errmsg(pDatabase));
			throw WALLET_STORE_EXCEPTION("Failed to create wallet.db");
		}

		const int version = VersionTable::GetCurrentVersion(*pDatabase);
		if (version == 0)
		{
			SqliteTransaction transaction(*pDatabase);
			transaction.Begin();

			VersionTable::CreateTable(*pDatabase);
			OutputsTable::UpdateSchema(*pDatabase, masterSeed, version);
			MetadataTable::UpdateSchema(*pDatabase, version);

			transaction.Commit();
		}
		else if (version > LATEST_SCHEMA_VERSION)
		{
			throw WALLET_STORE_EXCEPTION("Sqlite database has higher version than supported.");
		}
	}
	catch (std::exception&)
	{
		sqlite3_close(pDatabase);
		throw;
	}

	Locked<IWalletDB> walletDB(std::shared_ptr<IWalletDB>(new WalletSqlite(m_walletDirectory, username, pDatabase)));
	m_userDBs.insert(std::make_pair(username, walletDB));
	return walletDB;
}

Locked<IWalletDB> SqliteStore::CreateWallet(const std::string& username, const EncryptedSeed& encryptedSeed)
{
	const fs::path userDBPath = m_walletDirectory / username;
	const fs::path seedFile = userDBPath / "seed.json";
	if (FileUtil::Exists(seedFile))
	{
		WALLET_WARNING("Wallet already exists for user: " + username);
		throw WALLET_STORE_EXCEPTION("Wallet already exists.");
	}

	if (!FileUtil::CreateDirectories(userDBPath))
	{
		WALLET_ERROR_F("Failed to create wallet directory for user: {}", username);
		throw WALLET_STORE_EXCEPTION("Failed to create directory.");
	}

	const std::string seedJSON = encryptedSeed.ToJSON().toStyledString();
	const std::vector<unsigned char> seedBytes(seedJSON.data(), seedJSON.data() + seedJSON.size());
	FileUtil::SafeWriteToFile(seedFile, seedBytes);

	sqlite3* pDatabase = CreateWalletDB(username);

	Locked<IWalletDB> walletDB(std::shared_ptr<IWalletDB>(new WalletSqlite(m_walletDirectory, username, pDatabase)));
	m_userDBs.insert(std::make_pair(username, walletDB));
	return walletDB;
}

void SqliteStore::ChangePassword(const std::string& username, const EncryptedSeed& encryptedSeed)
{
	const fs::path userDBPath = m_walletDirectory / username;
	const fs::path seedFile = userDBPath / "seed.json";

	const std::string seedJSON = encryptedSeed.ToJSON().toStyledString();
	const std::vector<unsigned char> seedBytes(seedJSON.data(), seedJSON.data() + seedJSON.size());
	FileUtil::SafeWriteToFile(seedFile, seedBytes);
}

sqlite3* SqliteStore::CreateWalletDB(const std::string& username)
{
	const fs::path walletDBFile = GetDBFile(username);
	sqlite3* pDatabase = nullptr;
	if (sqlite3_open(walletDBFile.u8string().c_str(), &pDatabase) != SQLITE_OK)
	{
		WALLET_ERROR_F("Failed to create wallet.db for user ({}) with error ({})", username, sqlite3_errmsg(pDatabase));
		sqlite3_close(pDatabase);
		FileUtil::RemoveFile(walletDBFile);

		throw WALLET_STORE_EXCEPTION("Failed to create wallet.");
	}

	try
	{
		SqliteTransaction transaction(*pDatabase);
		transaction.Begin();

		VersionTable::CreateTable(*pDatabase);
		OutputsTable::CreateTable(*pDatabase);
		TransactionsTable::CreateTable(*pDatabase);
		MetadataTable::CreateTable(*pDatabase);

		std::string tableCreation = "create table accounts(parent_path TEXT PRIMARY KEY, account_name TEXT NOT NULL, next_child_index INTEGER NOT NULL);";
		tableCreation += "create table slate_contexts(slate_id TEXT PRIMARY KEY, enc_blind BLOB NOT NULL, enc_nonce BLOB NOT NULL);";
		// TODO: Add indices

		KeyChainPath nextChildPath = KeyChainPath::FromString("m/0/0").GetRandomChild();
		tableCreation += "insert into accounts values('m/0/0','DEFAULT'," + std::to_string(nextChildPath.GetKeyIndices().back()) + ");";

		char* error = nullptr;
		if (sqlite3_exec(pDatabase, tableCreation.c_str(), NULL, NULL, &error) != SQLITE_OK)
		{
			WALLET_ERROR_F("Failed to create DB tables for user: {}", username);
			sqlite3_free(error);
		}

		transaction.Commit();
	}
	catch (WalletStoreException&)
	{
		sqlite3_close(pDatabase);
		FileUtil::RemoveFile(walletDBFile);
		throw;
	}

	return pDatabase;
}

EncryptedSeed SqliteStore::LoadWalletSeed(const std::string& username) const
{
	const auto wideUsername = StringUtil::ToWide(username);

	WALLET_TRACE_F("Loading wallet seed for {}", wideUsername);

	const fs::path seedPath = m_walletDirectory / wideUsername / "seed.json";
	if (!FileUtil::Exists(seedPath))
	{
		WALLET_WARNING_F("Wallet not found for user: {}", wideUsername);
		throw WALLET_STORE_EXCEPTION("Wallet not found.");
	}

	std::vector<unsigned char> seedBytes;
	if (!FileUtil::ReadFile(seedPath, seedBytes))
	{
		WALLET_ERROR_F("Failed to load seed.json for user: {}", wideUsername);
		throw WALLET_STORE_EXCEPTION("Failed to load seed.json");
	}

	std::string seed(seedBytes.data(), seedBytes.data() + seedBytes.size());

	Json::Value seedJSON;
	if (!JsonUtil::Parse(seed, seedJSON))
	{
		WALLET_ERROR_F("Failed to deserialize seed.json for user: {}", wideUsername);
		throw WALLET_STORE_EXCEPTION("Failed to deserialize seed.json");
	}

	return EncryptedSeed::FromJSON(seedJSON);
}