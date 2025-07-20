#include "./PaymentService.h"
#include "./Util.h"


namespace rinhaback::api
{
	void PaymentService::postPayment(
		PaymentGateway gateway, double amount, const CorrelationId& correlationId, DateTimeMillis requestedAt)
	{
		auto& repository = repositories[std::to_underlying(gateway)];
		repository.postPayment(amount, correlationId, requestedAt);
	}

	PaymentService::PaymentsSummaryResponse PaymentService::getPaymentsSummary(
		std::optional<DateTimeMillis> from, std::optional<DateTimeMillis> to)
	{
		const std::optional<std::int64_t> fromInt =
			from.has_value() ? std::make_optional(from->time_since_epoch().count()) : std::nullopt;
		const std::optional<std::int64_t> toInt =
			to.has_value() ? std::make_optional(to->time_since_epoch().count()) : std::nullopt;

		Connection& connection = getConnection();
		Transaction transaction(connection, MDB_RDONLY);

		PaymentsSummaryResponse response{
			.defaultGateway = repositories[std::to_underlying(PaymentGateway::DEFAULT)].getPaymentsSummary(
				transaction, fromInt, toInt),
			.fallbackGateway = repositories[std::to_underlying(PaymentGateway::FALLBACK)].getPaymentsSummary(
				transaction, fromInt, toInt),
		};

		return response;
	};

	void PaymentService::purge()
	{
		repositories[std::to_underlying(PaymentGateway::DEFAULT)].purge();
		repositories[std::to_underlying(PaymentGateway::FALLBACK)].purge();
	}
}  // namespace rinhaback::api
