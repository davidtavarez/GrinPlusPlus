#pragma once

#include <Config/Config.h>
#include <BlockChain/BlockChainServer.h>
#include <P2P/ConnectedPeer.h>

// Forward Declarations
class Socket;
class PeerManager;
class ConnectionManager;

class HandShake
{
public:
	HandShake(const Config& config, ConnectionManager& connectionManager, PeerManager& peerManager, IBlockChainServerPtr pBlockChainServer);

	bool PerformHandshake(Socket& socket, ConnectedPeer& connectedPeer, const EDirection direction) const;

private:
	bool PerformOutboundHandshake(Socket& socket, ConnectedPeer& connectedPeer) const;
	bool PerformInboundHandshake(Socket& socket, ConnectedPeer& connectedPeer) const;
	bool TransmitHandMessage(Socket& socket) const;
	bool TransmitShakeMessage(Socket& socket) const;

	const Config& m_config;
	ConnectionManager& m_connectionManager;
	PeerManager& m_peerManager;
	IBlockChainServerPtr m_pBlockChainServer;
};