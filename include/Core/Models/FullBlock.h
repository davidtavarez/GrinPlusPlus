#pragma once

// Copyright (c) 2018-2019 David Burkett
// Distributed under the MIT software license, see the accompanying
// file LICENSE or http://www.opensource.org/licenses/mit-license.php.

#include <vector>
#include <Core/Models/BlockHeader.h>
#include <Core/Models/TransactionBody.h>
#include <Core/Serialization/ByteBuffer.h>
#include <Core/Serialization/Serializer.h>
#include <Common/Traits.h>

class FullBlock : public Traits::IPrintable
{
public:
	//
	// Constructors
	//
	FullBlock(BlockHeader&& blockHeader, TransactionBody&& transactionBody);
	FullBlock(const FullBlock& other) = default;
	FullBlock(FullBlock&& other) noexcept = default;
	FullBlock() = default;

	//
	// Destructor
	//
	~FullBlock() = default;

	//
	// Operators
	//
	FullBlock& operator=(const FullBlock& other) = default;
	FullBlock& operator=(FullBlock&& other) noexcept = default;

	//
	// Getters
	//
	inline const BlockHeader& GetBlockHeader() const { return m_blockHeader; }
	inline const TransactionBody& GetTransactionBody() const { return m_transactionBody; }
	inline uint64_t GetHeight() const { return m_blockHeader.GetHeight(); }

	//
	// Serialization/Deserialization
	//
	void Serialize(Serializer& serializer) const;
	static FullBlock Deserialize(ByteBuffer& byteBuffer);

	//
	// Hashing
	//
	inline const Hash& GetHash() const { return m_blockHeader.GetHash(); }

	//
	// Validation Status
	//
	inline bool WasValidated() const { return m_validated; }
	inline void MarkAsValidated() const { m_validated = true; }

	//
	// Traits
	//
	virtual std::string Format() const override final { return GetHash().ToHex(); }

private:
	BlockHeader m_blockHeader;
	TransactionBody m_transactionBody;
	mutable bool m_validated;
};