#pragma once

#include <Config/Config.h>
#include <Wallet/WalletDB/WalletDB.h>
#include <Common/ImportExport.h>

#ifdef MW_WalletDB
#define WALLET_DB_API EXPORT
#else
#define WALLET_DB_API IMPORT
#endif

class IWalletStore
{
public:
	virtual ~IWalletStore() = default;

	virtual std::vector<std::string> GetAccounts() const = 0;

	virtual Locked<IWalletDB> OpenWallet(const std::string& username, const SecureVector& masterSeed) = 0;
	virtual Locked<IWalletDB> CreateWallet(const std::string& username, const EncryptedSeed& encryptedSeed) = 0;
	virtual void ChangePassword(const std::string& username, const EncryptedSeed& encryptedSeed) = 0;
	virtual EncryptedSeed LoadWalletSeed(const std::string& username) const = 0;
};

namespace WalletDBAPI
{
	//
	// Opens the wallet database and returns an instance of IWalletStore.
	//
	WALLET_DB_API std::shared_ptr<IWalletStore> OpenWalletDB(const Config& config);
}