#pragma once

#include "./Database.h"
#include "./PaymentRepository.h"
#include "./Util.h"
#include <optional>
#include <utility>


namespace rinhaback::api
{
	class PaymentService final
	{
	public:
		struct PaymentsSummaryResponse
		{
			PaymentRepository::PaymentsGatewaySummaryResponse defaultGateway;
			PaymentRepository::PaymentsGatewaySummaryResponse fallbackGateway;
		};

	public:
		PaymentService() { }

	public:
		void postPayment(
			PaymentGateway gateway, double amount, const CorrelationId& correlationId, DateTimeMillis requestedAt);
		PaymentsSummaryResponse getPaymentsSummary(
			std::optional<DateTimeMillis> from, std::optional<DateTimeMillis> to);

		void purge();

	private:
		PaymentRepository repositories[std::to_underlying(PaymentGateway::SIZE)] = {
			{PaymentGateway::DEFAULT}, {PaymentGateway::FALLBACK}};
	};
}  // namespace rinhaback::api
