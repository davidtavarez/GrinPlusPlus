#include <catch.hpp>

#include <TestServer.h>
#include <TestMiner.h>
#include <TestChain.h>
#include <TxBuilder.h>
#include <TestHelper.h>

#include <BlockChain/BlockChainServer.h>
#include <Database/Database.h>
#include <PMMR/TxHashSetManager.h>
#include <TxPool/TransactionPool.h>
#include <Core/Validation/TransactionValidator.h>
#include <Core/File/FileRemover.h>
#include <Consensus/Common.h>
#include <Core/Util/TransactionUtil.h>

//
// a - b - c
//  \
//   - b'
//
// Process in the following order -
// 1. block_a
// 2. header_b
// 3. header_b_fork
// 4. block_b_fork
// 5. block_b
// 6. block_c
//
TEST_CASE("REORG 1")
{
	TestServer::Ptr pTestServer = TestServer::Create();
	KeyChain keyChain = KeyChain::FromRandom(*pTestServer->GetConfig());
	TxBuilder txBuilder(keyChain);
	auto pBlockChainServer = pTestServer->GetBlockChainServer();

	TestChain chain1(pTestServer, keyChain);

	Test::Tx coinbase_a = txBuilder.BuildCoinbaseTx(KeyChainPath({ 0, 1 }));
	MinedBlock block_a = chain1.AddNextBlock({ coinbase_a });

	Test::Tx coinbase_b = txBuilder.BuildCoinbaseTx(KeyChainPath({ 0, 2 }));
	MinedBlock block_b = chain1.AddNextBlock({ coinbase_b });

	Test::Tx coinbase_c = txBuilder.BuildCoinbaseTx(KeyChainPath({ 0, 3 }));
	MinedBlock block_c = chain1.AddNextBlock({ coinbase_c });

	chain1.Rewind(2);
	Test::Tx coinbase_b_fork = txBuilder.BuildCoinbaseTx(KeyChainPath({ 1, 2 }));
	MinedBlock block_b_fork = chain1.AddNextBlock({ coinbase_b_fork });

	////////////////////////////////////////
	// 1. block_a
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlock(block_a.block) == EBlockChainStatus::SUCCESS);

	REQUIRE(pBlockChainServer->GetHeight(EChainType::CONFIRMED) == 1);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_a.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetHeight(EChainType::CANDIDATE) == 1);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_a.block.GetHeader()->GetHash());

	////////////////////////////////////////
	// 2. header_b
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlockHeader(block_b.block.GetHeader()) == EBlockChainStatus::SUCCESS);

	REQUIRE(pBlockChainServer->GetHeight(EChainType::CONFIRMED) == 1);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_a.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetHeight(EChainType::CANDIDATE) == 2);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_b.block.GetHeader()->GetHash());

	////////////////////////////////////////
	// 3. header_b_fork
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlockHeader(block_b_fork.block.GetHeader()) == EBlockChainStatus::SUCCESS);

	REQUIRE(pBlockChainServer->GetHeight(EChainType::CONFIRMED) == 1);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_a.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetHeight(EChainType::CANDIDATE) == 2);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_b.block.GetHeader()->GetHash());

	////////////////////////////////////////
	// 4. block_b_fork
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlock(block_b_fork.block) == EBlockChainStatus::SUCCESS);

	REQUIRE(pBlockChainServer->GetHeight(EChainType::CONFIRMED) == 2);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_b_fork.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetHeight(EChainType::CANDIDATE) == 2);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_b.block.GetHeader()->GetHash());

	////////////////////////////////////////
	// 5. block_b
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlock(block_b.block) == EBlockChainStatus::SUCCESS);

	REQUIRE(pBlockChainServer->GetHeight(EChainType::CONFIRMED) == 2);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_b_fork.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetHeight(EChainType::CANDIDATE) == 2);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_b.block.GetHeader()->GetHash());

	////////////////////////////////////////
	// 6. block_c
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlock(block_c.block) == EBlockChainStatus::SUCCESS);

	REQUIRE(pBlockChainServer->GetHeight(EChainType::CONFIRMED) == 3);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_c.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetHeight(EChainType::CANDIDATE) == 3);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_c.block.GetHeader()->GetHash());
}

//
// a - b - c
//  \
//   - b'
//
// Process in the following order -
// 1. block_a
// 2. header_b
// 3. header_c
// 4. block_b_fork
// 5. block_c
// 6. block_b
//
TEST_CASE("REORG 2")
{
	TestServer::Ptr pTestServer = TestServer::Create();
	KeyChain keyChain = KeyChain::FromRandom(*pTestServer->GetConfig());
	TxBuilder txBuilder(keyChain);
	auto pBlockChainServer = pTestServer->GetBlockChainServer();

	TestChain chain1(pTestServer, keyChain);

	Test::Tx coinbase_a = txBuilder.BuildCoinbaseTx(KeyChainPath({ 0, 1 }));
	MinedBlock block_a = chain1.AddNextBlock({ coinbase_a });

	Test::Tx coinbase_b = txBuilder.BuildCoinbaseTx(KeyChainPath({ 0, 2 }));
	MinedBlock block_b = chain1.AddNextBlock({ coinbase_b });

	Test::Tx coinbase_c = txBuilder.BuildCoinbaseTx(KeyChainPath({ 0, 3 }));
	MinedBlock block_c = chain1.AddNextBlock({ coinbase_c });

	chain1.Rewind(2);
	Test::Tx coinbase_b_fork = txBuilder.BuildCoinbaseTx(KeyChainPath({ 1, 2 }));
	MinedBlock block_b_fork = chain1.AddNextBlock({ coinbase_b_fork });

	////////////////////////////////////////
	// 1. block_a
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlock(block_a.block) == EBlockChainStatus::SUCCESS);

	REQUIRE(pBlockChainServer->GetHeight(EChainType::CONFIRMED) == 1);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_a.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetHeight(EChainType::CANDIDATE) == 1);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_a.block.GetHeader()->GetHash());

	////////////////////////////////////////
	// 2. header_b
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlockHeader(block_b.block.GetHeader()) == EBlockChainStatus::SUCCESS);

	REQUIRE(pBlockChainServer->GetHeight(EChainType::CONFIRMED) == 1);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_a.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetHeight(EChainType::CANDIDATE) == 2);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_b.block.GetHeader()->GetHash());

	////////////////////////////////////////
	// 3. header_c
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlockHeader(block_c.block.GetHeader()) == EBlockChainStatus::SUCCESS);

	REQUIRE(pBlockChainServer->GetHeight(EChainType::CONFIRMED) == 1);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_a.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetHeight(EChainType::CANDIDATE) == 3);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_c.block.GetHeader()->GetHash());

	////////////////////////////////////////
	// 4. block_b_fork
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlock(block_b_fork.block) == EBlockChainStatus::SUCCESS);

	REQUIRE(pBlockChainServer->GetHeight(EChainType::CONFIRMED) == 2);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_b_fork.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetHeight(EChainType::CANDIDATE) == 3);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_c.block.GetHeader()->GetHash());

	////////////////////////////////////////
	// 5. block_c
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlock(block_c.block) == EBlockChainStatus::ORPHANED);

	REQUIRE(pBlockChainServer->GetHeight(EChainType::CONFIRMED) == 2);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_b_fork.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetHeight(EChainType::CANDIDATE) == 3);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_c.block.GetHeader()->GetHash());

	////////////////////////////////////////
	// 6. block_b
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlock(block_b.block) == EBlockChainStatus::SUCCESS);

	REQUIRE(pBlockChainServer->GetHeight(EChainType::CONFIRMED) == 2);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_b_fork.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetHeight(EChainType::CANDIDATE) == 3);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_c.block.GetHeader()->GetHash());

	////////////////////////////////////////
	// 7. Process orphans
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->ProcessNextOrphanBlock());

	REQUIRE(pBlockChainServer->GetHeight(EChainType::CONFIRMED) == 3);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_c.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetHeight(EChainType::CANDIDATE) == 3);
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_c.block.GetHeader()->GetHash());
}

//
// a - b - c
//  \
//   - b'
//
// Process in the following order -
// 1. block_a
// 2. block_b
// 3. block_c
// 4. block_b_fork - higher difficulty
//
TEST_CASE("REORG 3")
{
	TestServer::Ptr pTestServer = TestServer::Create();
	KeyChain keyChain = KeyChain::FromRandom(*pTestServer->GetConfig());
	TxBuilder txBuilder(keyChain);
	auto pBlockChainServer = pTestServer->GetBlockChainServer();

	TestChain chain1(pTestServer, keyChain);

	Test::Tx coinbase_a = txBuilder.BuildCoinbaseTx(KeyChainPath({ 0, 1 }));
	MinedBlock block_a = chain1.AddNextBlock({ coinbase_a });

	Test::Tx coinbase_b = txBuilder.BuildCoinbaseTx(KeyChainPath({ 0, 2 }));
	MinedBlock block_b = chain1.AddNextBlock({ coinbase_b });

	Test::Tx coinbase_c = txBuilder.BuildCoinbaseTx(KeyChainPath({ 0, 3 }));
	MinedBlock block_c = chain1.AddNextBlock({ coinbase_c });

	chain1.Rewind(2);
	Test::Tx coinbase_b_fork = txBuilder.BuildCoinbaseTx(KeyChainPath({ 1, 2 }));
	MinedBlock block_b_fork = chain1.AddNextBlock({ coinbase_b_fork }, 10);

	////////////////////////////////////////
	// Process block_a, block_b, block_c
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlock(block_a.block) == EBlockChainStatus::SUCCESS);
	REQUIRE(pBlockChainServer->AddBlock(block_b.block) == EBlockChainStatus::SUCCESS);
	REQUIRE(pBlockChainServer->AddBlock(block_c.block) == EBlockChainStatus::SUCCESS);

	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_c.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_c.block.GetHeader()->GetHash());

	////////////////////////////////////////
	// Process forked block_b with higher difficulty
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlock(block_b_fork.block) == EBlockChainStatus::SUCCESS);

	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CONFIRMED)->GetHash() == block_b_fork.block.GetHeader()->GetHash());
	REQUIRE(pBlockChainServer->GetTipBlockHeader(EChainType::CANDIDATE)->GetHash() == block_b_fork.block.GetHeader()->GetHash());
}

TEST_CASE("Reorg Chain")
{
	TestServer::Ptr pTestServer = TestServer::Create();
	TestMiner miner(pTestServer);
	KeyChain keyChain = KeyChain::FromRandom(*pTestServer->GetConfig());
	TxBuilder txBuilder(keyChain);
	auto pBlockChainServer = pTestServer->GetBlockChainServer();

	////////////////////////////////////////
	// Mine a chain with 30 blocks (Coinbase maturity for tests is only 25)
	////////////////////////////////////////
	std::vector<MinedBlock> minedChain = miner.MineChain(keyChain, 30);
	REQUIRE(minedChain.size() == 30);

	// Create a transaction that spends the coinbase from block 1
	TransactionOutput outputToSpend = minedChain[1].block.GetOutputs().front();
	Test::Input input({
		{ outputToSpend.GetFeatures(), outputToSpend.GetCommitment() },
		minedChain[1].coinbasePath.value(),
		minedChain[1].coinbaseAmount
	});
	Test::Output newOutput({
		KeyChainPath({ 1, 0 }),
		(uint64_t)10'000'000
	});
	Test::Output changeOutput({
		KeyChainPath({ 1, 1 }),
		(uint64_t)(minedChain[1].coinbaseAmount - 10'000'000)
	});

	Transaction spendTransaction = txBuilder.BuildTx(0, { input }, { newOutput, changeOutput });

	// Create block 30a
	Test::Tx coinbaseTx30a = txBuilder.BuildCoinbaseTx(KeyChainPath({ 0, 30 }));

	// Combine transaction with coinbase transaction for block 29b
	TransactionPtr pCombinedTx30a = TransactionUtil::Aggregate({
		coinbaseTx30a.pTransaction,
		std::make_shared<Transaction>(spendTransaction)
	});

	FullBlock block30a = miner.MineNextBlock(
		minedChain.back().block.GetBlockHeader(),
		*pCombinedTx30a
	);

	REQUIRE(pBlockChainServer->AddBlock(block30a) == EBlockChainStatus::SUCCESS);

	////////////////////////////////////////
	// Create "reorg" chain with block 28b that spends an earlier coinbase, and blocks 29b, 30b, and 31b on top
	////////////////////////////////////////
	Test::Tx coinbaseTx28b = txBuilder.BuildCoinbaseTx(KeyChainPath({ 1, 28 }));

	// Combine transaction with coinbase transaction for block 29b
	TransactionPtr pCombinedTx28b = TransactionUtil::Aggregate({
		coinbaseTx28b.pTransaction,
		std::make_shared<Transaction>(spendTransaction)
	});

	// Create block 28b
	FullBlock block28b = miner.MineNextBlock(
		minedChain[27].block.GetBlockHeader(),
		*pCombinedTx28b
	);

	// Create block 29b
	Test::Tx coinbaseTx29b = txBuilder.BuildCoinbaseTx(KeyChainPath({ 1, 29 }));
	FullBlock block29b = miner.MineNextBlock(
		minedChain[27].block.GetBlockHeader(),
		*coinbaseTx29b.pTransaction,
		{ block28b }
	);

	// Create block 30b
	Test::Tx coinbaseTx30b = txBuilder.BuildCoinbaseTx(KeyChainPath({ 1, 30 }));
	FullBlock block30b = miner.MineNextBlock(
		minedChain[27].block.GetBlockHeader(),
		*coinbaseTx30b.pTransaction,
		{ block28b, block29b }
	);

	// Create block 31b
	Test::Tx coinbaseTx31b = txBuilder.BuildCoinbaseTx(KeyChainPath({ 1, 31 }));
	FullBlock block31b = miner.MineNextBlock(
		minedChain[27].block.GetBlockHeader(),
		*coinbaseTx31b.pTransaction,
		{ block28b, block29b, block30b }
	);

	////////////////////////////////////////
	// Verify that block31a is added, but then replaced successfully with block31b & block32b
	////////////////////////////////////////
	REQUIRE(pBlockChainServer->AddBlock(block28b) == EBlockChainStatus::SUCCESS);
	REQUIRE(pBlockChainServer->AddBlock(block29b) == EBlockChainStatus::SUCCESS);
	REQUIRE(pBlockChainServer->AddBlock(block30b) == EBlockChainStatus::SUCCESS);
	REQUIRE(pBlockChainServer->AddBlock(block31b) == EBlockChainStatus::SUCCESS);

	REQUIRE(pBlockChainServer->GetBlockByHeight(31)->GetHash() == block31b.GetHash());

	// TODO: Assert unspent positions in leafset and in database.
}