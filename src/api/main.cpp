#include "mimalloc-new-delete.h"
#include "./PaymentProcessor.h"
#include "./Config.h"
#include "./GatewayChooserService.h"
#include "./PendingPaymentsQueue.h"
#include "./SignalHandling.h"
#include "./Util.h"
#include <array>
#include <atomic>
#include <exception>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <experimental/scope>
#include "mongoose.h"
#include "yyjson.h"


namespace rinhaback::api
{
	static constexpr auto RESPONSE_HEADERS = "Content-Type: application/json\r\n";

	static const auto MG_GET = mg_str("GET");
	static const auto MG_POST = mg_str("POST");
	static const auto MG_PURGE_PAYMENTS_PATH = mg_str("/purge-payments");
	static const auto MG_PAYMENTS_SUMMARY_PATH = mg_str("/payments-summary");
	static const auto MG_PAYMENTS_PATH = mg_str("/payments");

	static std::shared_ptr<PaymentService> paymentService{std::make_shared<PaymentService>()};
	static std::shared_ptr<PendingPaymentsQueue> pendingPaymentsQueue{std::make_shared<PendingPaymentsQueue>()};

	static void httpHandler(mg_connection* conn, int ev, void* evData)
	{
		struct Response
		{
			int statusCode = 0;
			std::array<char, 2000> json = {'{', '}', '\0'};
		};

		try
		{
			if (ev == MG_EV_HTTP_MSG)
			{
				const auto httpMessage = static_cast<mg_http_message*>(evData);
				const bool isGet = mg_strcmp(httpMessage->method, MG_GET) == 0;
				const bool isPost = !isGet && mg_strcmp(httpMessage->method, MG_POST) == 0;

				if (isGet && mg_match(httpMessage->uri, MG_PAYMENTS_SUMMARY_PATH, nullptr))
				{
					Response response;

					std::experimental::scope_exit scopeExit([&]()
						{ mg_http_reply(conn, response.statusCode, RESPONSE_HEADERS, "%s", response.json.begin()); });

					std::optional<DateTimeMillis> from, to;
					char queryParamBuffer[100];

					if (mg_http_get_var(&httpMessage->query, "from", queryParamBuffer, sizeof(queryParamBuffer)) > 0)
						from = parseDateTime(queryParamBuffer);

					if (mg_http_get_var(&httpMessage->query, "to", queryParamBuffer, sizeof(queryParamBuffer)) > 0)
						to = parseDateTime(queryParamBuffer);

					const auto summary = paymentService->getPaymentsSummary(from, to);

					const auto& defaultGateway = summary.defaultGateway;
					const auto& fallbackGateway = summary.fallbackGateway;

					std::format_to_n(response.json.begin(), response.json.size(),
						R"({{"default":{{"totalRequests":{},"totalAmount":{:.2f}}},)"
						R"("fallback":{{"totalRequests":{},"totalAmount":{:.2f}}}}})",
						defaultGateway.totalRequests, defaultGateway.totalAmount, fallbackGateway.totalRequests,
						fallbackGateway.totalAmount);

					response.statusCode = HTTP_STATUS_OK;
				}
				else if (isPost && mg_match(httpMessage->uri, MG_PAYMENTS_PATH, nullptr))
				{
					Response response;
					response.statusCode = HTTP_STATUS_UNPROCESSABLE_CONTENT;

					const auto inDocJson = yyjson_read(httpMessage->body.buf, httpMessage->body.len, 0);

					std::experimental::scope_exit scopeExit(
						[&]()
						{
							if (response.statusCode != HTTP_STATUS_OK)
								mg_http_reply(conn, response.statusCode, RESPONSE_HEADERS, "");

							yyjson_doc_free(inDocJson);
						});

					const auto inRootJson = yyjson_doc_get_root(inDocJson);
					const auto correlationIdJson = yyjson_obj_get(inRootJson, "correlationId");
					const auto amountJson = yyjson_obj_get(inRootJson, "amount");

					if (yyjson_is_str(correlationIdJson) && yyjson_is_num(amountJson))
					{
						const std::span correlationId(
							yyjson_get_str(correlationIdJson), yyjson_get_len(correlationIdJson));
						const auto amount = yyjson_get_num(amountJson);

						if (correlationId.size() == std::tuple_size<CorrelationId>() && amount > 0)
						{
							response.statusCode = HTTP_STATUS_OK;
							mg_http_reply(conn, response.statusCode, RESPONSE_HEADERS, "");

							PendingPaymentsQueue::Payment pendingPayment = {.amount = amount};
							std::copy_n(correlationId.data(), pendingPayment.correlationId.size(),
								pendingPayment.correlationId.begin());

							pendingPaymentsQueue->enqueue(pendingPayment);
						}
					}
				}
				else if (isPost && mg_match(httpMessage->uri, MG_PURGE_PAYMENTS_PATH, nullptr))
				{
					paymentService->purge();
					pendingPaymentsQueue->purge();

					mg_http_reply(conn, HTTP_STATUS_OK, RESPONSE_HEADERS, "");
				}
				else
				{
					mg_http_reply(conn, HTTP_STATUS_INTERNAL_SERVER_ERROR, RESPONSE_HEADERS, "{%m:%m}\n",
						MG_ESC("error"), MG_ESC("Unsupported URI"));
				}
			}
		}
		catch (const std::exception& e)
		{
			std::println(stderr, "{}", e.what());
		}
		catch (...)
		{
			std::println(stderr, "Unknown exception");
		}
	}

	static int run(int argc, const char* argv[])
	{
		SignalHandling::install();

		std::vector<std::jthread> threads;
		threads.reserve(1 + Config::processorWorkers + Config::serverWorkers);

		if (Config::databaseInit)
			threads.emplace_back(GatewayChooserService::start());

		for (unsigned i = 0; i < Config::processorWorkers; ++i)
			threads.emplace_back(PaymentProcessor::start(pendingPaymentsQueue, paymentService));

		for (unsigned i = 0; i < Config::serverWorkers; ++i)
		{
			threads.emplace_back(
				[]
				{
					mg_mgr mgr;
					mg_mgr_init(&mgr);
					mg_http_listen(&mgr, Config::listenAddress.c_str(), httpHandler, nullptr);

					while (!SignalHandling::shouldFinish())
						mg_mgr_poll(&mgr, Config::serverPollTime);

					mg_mgr_free(&mgr);
				});
		}

		getConnection();

		std::println("Server listening on {}", Config::listenAddress);

		threads.clear();

		std::println("Exiting");

		return 0;
	}
}  // namespace rinhaback::api

int main(const int argc, const char* argv[])
{
	using namespace rinhaback::api;

	try
	{
		return run(argc, argv);
	}
	catch (const std::exception& e)
	{
		std::println(stderr, "{}", e.what());
		return 1;
	}
}
