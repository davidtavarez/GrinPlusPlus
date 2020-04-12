#pragma once

#include <Config/Config.h>
#include <Wallet/SessionToken.h>
#include <Net/Tor/TorAddress.h>
#include <API/Wallet/Foreign/ForeignServer.h>
#include <civetweb.h>
#include <unordered_map>
#include <optional>
#include <string>
#include <mutex>

// Forward Declarations
class IWalletManager;

class ForeignController
{
public:
	ForeignController(const Config& config, IWalletManager& walletManager);
	~ForeignController();

	std::pair<uint16_t, std::optional<TorAddress>> StartListener(
		const std::string& username,
		const SessionToken& token,
		const SecureVector& seed
	);
	bool StopListener(const std::string& username);

private:
	struct Context
	{
		Context(const ForeignServer::Ptr& pServer)
			: m_numReferences(1), m_pServer(pServer) { }

		int m_numReferences;
		ForeignServer::Ptr m_pServer;
	};

	const Config& m_config;
	IWalletManager& m_walletManager;

	mutable std::mutex m_contextsMutex;
	std::unordered_map<std::string, std::shared_ptr<Context>> m_contextsByUsername;
};