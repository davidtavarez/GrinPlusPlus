#pragma once

#include <Core/Models/FullBlock.h>
#include <Core/Models/BlockSums.h>
#include <Database/BlockDb.h>
#include <Core/Traits/Lockable.h>
#include <PMMR/TxHashSet.h>
#include <memory>

// Forward Declarations
class BlindingFactor;

class BlockValidator
{
public:
	BlockValidator(std::shared_ptr<const IBlockDB> pBlockDB, ITxHashSetConstPtr pTxHashSet);

	std::unique_ptr<BlockSums> ValidateBlock(const FullBlock& block) const;
	bool IsBlockSelfConsistent(const FullBlock& block) const;

private:
	bool VerifyKernelLockHeights(const FullBlock& block) const;
	bool VerifyCoinbase(const FullBlock& block) const;

	std::shared_ptr<const IBlockDB> m_pBlockDB;
	ITxHashSetConstPtr m_pTxHashSet;
};