#pragma once

#include <Database/Database.h>
#include <Core/Traits/Lockable.h>

#include "BlockDBImpl.h"
#include "PeerDBImpl.h"

class Database : public IDatabase
{
public:
	static std::shared_ptr<IDatabase> Open(const Config& config);

	virtual std::shared_ptr<Locked<IBlockDB>> GetBlockDB() override final { return m_pBlockDB; }
	virtual std::shared_ptr<const Locked<IBlockDB>> GetBlockDB() const override final { return m_pBlockDB; }
	virtual std::shared_ptr<Locked<IPeerDB>> GetPeerDB() override final { return m_pPeerDB; }
	virtual std::shared_ptr<const Locked<IPeerDB>> GetPeerDB() const override final { return m_pPeerDB; }

private:
	Database(const Config& config, std::shared_ptr<Locked<IBlockDB>> pBlockDB, std::shared_ptr<Locked<IPeerDB>> pPeerDB);

	const Config& m_config;

	std::shared_ptr<Locked<IBlockDB>> m_pBlockDB;
	std::shared_ptr<Locked<IPeerDB>> m_pPeerDB;
};