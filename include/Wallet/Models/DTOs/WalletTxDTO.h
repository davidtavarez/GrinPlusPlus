#pragma once

#include <Common/Util/TimeUtil.h>
#include <Core/Util/JsonUtil.h>
#include <Wallet/WalletTx.h>
#include <Wallet/Models/DTOs/WalletOutputDTO.h>

class WalletTxDTO
{
public:
	WalletTxDTO(const WalletTx& walletTx, const std::vector<Commitment>& kernels, const std::vector<WalletOutputDTO>& outputs)
		: m_walletTx(walletTx), m_kernels(kernels), m_outputs(outputs)
	{

	}

	uint32_t GetId() const { return m_walletTx.GetId(); }

	Json::Value ToJSON() const
	{
		Json::Value transactionJSON;
		transactionJSON["id"] = m_walletTx.GetId();
		transactionJSON["type"] = WalletTxType::ToString(m_walletTx.GetType());
		transactionJSON["amount_credited"] = m_walletTx.GetAmountCredited();
		transactionJSON["amount_debited"] = m_walletTx.GetAmountDebited();
		transactionJSON["creation_date_time"] = TimeUtil::ToSeconds(m_walletTx.GetCreationTime());

		JsonUtil::AddOptionalField(transactionJSON, "address", m_walletTx.GetAddress());
		JsonUtil::AddOptionalField(transactionJSON, "slate_message", m_walletTx.GetSlateMessage());
		JsonUtil::AddOptionalField(transactionJSON, "fee", m_walletTx.GetFee());
		JsonUtil::AddOptionalField(transactionJSON, "confirmed_height", m_walletTx.GetConfirmationHeight());

		if (m_walletTx.GetSlateId().has_value())
		{
			transactionJSON["slate_id"] = uuids::to_string(m_walletTx.GetSlateId().value());
		}

		if (m_walletTx.GetConfirmationTime().has_value())
		{
			transactionJSON["confirmation_date_time"] = TimeUtil::ToSeconds(m_walletTx.GetConfirmationTime().value());
		}

		Json::Value kernelsJSON;
		for (const auto& kernel : m_kernels)
		{
			Json::Value kernelJSON;
			kernelJSON["commitment"] = kernel.ToHex();
			kernelsJSON.append(kernelJSON);
		}

		transactionJSON["kernels"] = kernelsJSON;

		Json::Value outputsJSON;
		for (const WalletOutputDTO& output : m_outputs)
		{
			outputsJSON.append(output.ToJSON());
		}

		transactionJSON["outputs"] = outputsJSON;

		return transactionJSON;
	}

private:
	WalletTx m_walletTx;
	std::vector<Commitment> m_kernels;
	std::vector<WalletOutputDTO> m_outputs;
};