#include "./PaymentProcessor.h"
#include "./Config.h"
#include "./GatewayChooserService.h"
#include "./SignalHandling.h"
#include "./Util.h"
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <cassert>
#include "httplib.h"


namespace rinhaback::api
{
	std::jthread PaymentProcessor::start(
		std::shared_ptr<PendingPaymentsQueue> pendingPaymentsQueue, std::shared_ptr<PaymentService> paymentService)
	{
		const auto processor = std::make_shared<PaymentProcessor>();
		processor->pendingPaymentsQueue = std::move(pendingPaymentsQueue);
		processor->paymentService = std::move(paymentService);

		return std::jthread([processor]() { processor->handler(); });
	}

	void PaymentProcessor::handler()
	{
		std::println("PaymentProcessor started.");

		while (!SignalHandling::shouldFinish())
		{
			const auto optionalPayment = pendingPaymentsQueue->dequeue();

			if (optionalPayment.has_value())
				processPayment(optionalPayment.value());
		}

		std::println("PaymentProcessor stopped.");
	}

	void PaymentProcessor::processPayment(const PendingPaymentsQueue::Payment& payment)
	{
		if constexpr (false)
		{
			std::println("Processing payment: correlationId: {}, amount: {}",
				std::string_view(payment.correlationId.data(), payment.correlationId.size()), payment.amount);
		}

		std::optional<httplib::Client> httpClient;
		const std::string* url = nullptr;

		do
		{
			const auto gateway = GatewayChooserService::getGateway();
			const std::string* oldUrl = url;

			switch (gateway)
			{
				case PaymentGateway::DEFAULT:
					url = &Config::processorDefaultUrl;
					break;

				case PaymentGateway::FALLBACK:
					url = &Config::processorFallbackUrl;
					break;

				default:
					assert(false);
					break;
			}

			if (!oldUrl || url != oldUrl)
			{
				httpClient = httplib::Client(*url);
				httpClient->set_keep_alive(true);
			}

			const auto requestedAt = getCurrentDateTime();

			std::array<char, 2000> json;
			const auto jsonFormatResult = std::format_to_n(json.begin(), json.size(),
				R"({{"correlationId":"{}","amount":{:.2f},"requestedAt":"{:%FT%T}Z"}})",
				std::string_view(payment.correlationId.data(), payment.correlationId.size()), payment.amount,
				requestedAt);

			const auto httpResponse =
				httpClient->Post("/payments", json.data(), jsonFormatResult.size, HTTP_CONTENT_TYPE_JSON);
			const int httpStatus = httpResponse ? httpResponse->status : -1;

			if (httpStatus == HTTP_STATUS_OK)
			{
				if constexpr (false)
				{
					std::println("Payment processed successfully: correlationId: {}, amount: {}",
						std::string_view(payment.correlationId.data(), payment.correlationId.size()), payment.amount);
				}

				paymentService->postPayment(gateway, payment.amount, payment.correlationId, requestedAt);

				return;
			}
			else
			{
				if (!(httpStatus == -1 || (httpStatus >= 500 && httpStatus <= 599)))
				{
					GatewayChooserService::switchGatewayTo(
						gateway == PaymentGateway::DEFAULT ? PaymentGateway::FALLBACK : PaymentGateway::DEFAULT);

					if constexpr (false)
					{
						std::println("Payment processing failed: correlationId: {}, amount: {}, httpStatus: {}",
							std::string_view(payment.correlationId.data(), payment.correlationId.size()),
							payment.amount, httpStatus);
					}

					break;
				}
			}
		} while (true);
	}
}  // namespace rinhaback::api
