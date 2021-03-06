#pragma once

#include <Config/Config.h>
#include <Wallet/WalletManager.h>

// Forward Declarations
struct WalletContext;
struct mg_context;
struct mg_connection;

class WalletRestServer
{
public:
	~WalletRestServer();

	static std::shared_ptr<WalletRestServer> Create(const Config& config, IWalletManagerPtr pWalletManager, std::shared_ptr<INodeClient> pNodeClient);

private:
	WalletRestServer(
		const Config& config,
		IWalletManagerPtr pWalletManager,
		std::shared_ptr<WalletContext> pWalletContext,
		mg_context* pOwnerCivetContext
	);

	static int OwnerAPIHandler(mg_connection* pConnection, void* pWalletContext);

	const Config& m_config;
	IWalletManagerPtr m_pWalletManager;
	std::shared_ptr<WalletContext> m_pWalletContext;
	mg_context* m_pOwnerCivetContext;
};