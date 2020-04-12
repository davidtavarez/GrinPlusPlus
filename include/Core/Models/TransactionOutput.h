#pragma once

// Copyright (c) 2018-2019 David Burkett
// Distributed under the MIT software license, see the accompanying
// file LICENSE or http://www.opensource.org/licenses/mit-license.php.

#include <Core/Traits/Committed.h>
#include <Core/Traits/Hashable.h>
#include <Crypto/Hash.h>
#include <Core/Models/Features.h>
#include <Core/Models/OutputIdentifier.h>
#include <Crypto/Commitment.h>
#include <Crypto/RangeProof.h>
#include <Core/Serialization/ByteBuffer.h>
#include <Core/Serialization/Serializer.h>
#include <json/json.h>

////////////////////////////////////////
// TRANSACTION OUTPUT
////////////////////////////////////////
class TransactionOutput : public Traits::ICommitted, public Traits::IHashable, public Traits::IPrintable
{
public:
	//
	// Constructors
	//
	TransactionOutput(const EOutputFeatures features, Commitment&& commitment, RangeProof&& rangeProof);
	TransactionOutput(const TransactionOutput& transactionOutput) = default;
	TransactionOutput(TransactionOutput&& transactionOutput) noexcept = default;
	TransactionOutput() = default;

	//
	// Destructor
	//
	virtual ~TransactionOutput() = default;

	//
	// Operators
	//
	TransactionOutput& operator=(const TransactionOutput& transactionOutput) = default;
	TransactionOutput& operator=(TransactionOutput&& transactionOutput) noexcept = default;
	bool operator<(const TransactionOutput& transactionOutput) const { return GetCommitment() < transactionOutput.GetCommitment(); }
	bool operator==(const TransactionOutput& transactionOutput) const { return GetHash() == transactionOutput.GetHash(); }

	//
	// Getters
	//
	EOutputFeatures GetFeatures() const { return m_features; }
	virtual const Commitment& GetCommitment() const override final { return m_commitment; }
	const RangeProof& GetRangeProof() const { return m_rangeProof; }

	bool IsCoinbase() const { return (m_features & EOutputFeatures::COINBASE_OUTPUT) == EOutputFeatures::COINBASE_OUTPUT; }

	//
	// Serialization/Deserialization
	//
	void Serialize(Serializer& serializer) const;
	static TransactionOutput Deserialize(ByteBuffer& byteBuffer);
	Json::Value ToJSON() const;
	static TransactionOutput FromJSON(const Json::Value& transactionOutputJSON);

	//
	// Traits
	//
	virtual const Hash& GetHash() const override final;
	virtual std::string Format() const override final { return m_commitment.Format(); }

private:
	// Options for an output's structure or use
	EOutputFeatures m_features;

	// The homomorphic commitment representing the output amount
	Commitment m_commitment;

	// A proof that the commitment is in the right range
	RangeProof m_rangeProof;

	mutable Hash m_hash;
};

static struct
{
	bool operator()(const TransactionOutput& a, const TransactionOutput& b) const
	{
		return a.GetHash() < b.GetHash();
	}
} SortOutputsByHash;