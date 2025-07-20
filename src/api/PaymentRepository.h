#pragma once

#include "./Database.h"
#include "./Util.h"
#include <optional>
#include <cstdint>


namespace rinhaback::api
{
	class PaymentRepository final
	{
	public:
		struct PaymentsGatewaySummaryResponse
		{
			unsigned totalRequests;
			double totalAmount;
		};

	private:
		struct __attribute__((packed)) PaymentKey
		{
			std::int64_t dateTime;
		};

		struct __attribute__((packed)) PaymentData
		{
			double amount;
			CorrelationId correlationId;
		};

	public:
		PaymentRepository(PaymentGateway gateway)
			: gateway(gateway)
		{
		}

	public:
		void postPayment(double amount, const CorrelationId& correlationId, DateTimeMillis requestedAt);

		PaymentsGatewaySummaryResponse getPaymentsSummary(
			Transaction& transaction, std::optional<std::int64_t> from, std::optional<std::int64_t> to);

		void purge();

	private:
		PaymentGateway gateway;
	};
}  // namespace rinhaback::api
