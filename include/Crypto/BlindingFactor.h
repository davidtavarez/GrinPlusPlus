#pragma once

//
// This code is free for all purposes without any express guarantee it works.
//
// Author: David Burkett (davidburkett38@gmail.com)
//

#include <Crypto/Hash.h>
#include <Core/Serialization/ByteBuffer.h>
#include <Core/Serialization/Serializer.h>

class BlindingFactor
{
public:
	//
	// Constructors
	//
	BlindingFactor(Hash&& blindingFactorBytes)
		: m_blindingFactorBytes(std::move(blindingFactorBytes))
	{

	}
	BlindingFactor(const Hash& blindingFactorBytes)
		: m_blindingFactorBytes(blindingFactorBytes)
	{

	}
	BlindingFactor(const BlindingFactor& other) = default;
	BlindingFactor(BlindingFactor&& other) noexcept = default;

	//
	// Destructor
	//
	~BlindingFactor() = default;

	//
	// Operators
	//
	BlindingFactor& operator=(const BlindingFactor& other) = default;
	BlindingFactor& operator=(BlindingFactor&& other) noexcept = default;
	inline bool operator<(const BlindingFactor& rhs) const { return m_blindingFactorBytes < rhs.GetBytes(); }
	inline bool operator!=(const BlindingFactor& rhs) const { return m_blindingFactorBytes != rhs.GetBytes(); }
	inline bool operator==(const BlindingFactor& rhs) const { return m_blindingFactorBytes == rhs.GetBytes(); }

	//
	// Getters
	//
	inline const CBigInteger<32>& GetBytes() const { return m_blindingFactorBytes; }

	//
	// Serialization/Deserialization
	//
	void Serialize(Serializer& serializer) const
	{
		serializer.AppendBigInteger<32>(m_blindingFactorBytes);
	}

	static BlindingFactor Deserialize(ByteBuffer& byteBuffer)
	{
		return BlindingFactor(byteBuffer.ReadBigInteger<32>());
	}

private:
	// The 32 byte blinding factor.
	Hash m_blindingFactorBytes;
};