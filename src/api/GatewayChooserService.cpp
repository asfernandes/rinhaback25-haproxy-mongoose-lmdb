#include "./GatewayChooserService.h"
#include "./Config.h"
#include "./SignalHandling.h"
#include <atomic>
#include <cstdio>
#include <optional>
#include <print>
#include <string_view>
#include <utility>
#include <cassert>
#include <experimental/scope>
#include "boost/interprocess/shared_memory_object.hpp"
#include "boost/interprocess/mapped_region.hpp"
#include "httplib.h"
#include "yyjson.h"


namespace rinhaback::api
{
	namespace boostipc = boost::interprocess;

	namespace
	{
		struct SharedData
		{
			std::atomic_uint8_t currentGateway{static_cast<std::uint8_t>(PaymentGateway::DEFAULT)};
		};

		class SharedMemoryManager
		{
		private:
			static inline constexpr const char* SHARED_MEMORY_NAME =
				"rinhaback25-haproxy-mongoose-lmdb-GatewayChooserService";

		public:
			SharedMemoryManager(bool isCreator = false)
			{
				if (isCreator)
				{
					boostipc::shared_memory_object::remove(SHARED_MEMORY_NAME);
					shm =
						boostipc::shared_memory_object(boostipc::create_only, SHARED_MEMORY_NAME, boostipc::read_write);

					shm.truncate(sizeof(SharedData));

					region = boostipc::mapped_region(shm, boostipc::read_write);
					void* addr = region.get_address();
					data = new (addr) SharedData;
				}
				else
				{
					std::this_thread::sleep_for(std::chrono::seconds(2));  // FIXME:

					shm = boostipc::shared_memory_object(boostipc::open_only, SHARED_MEMORY_NAME, boostipc::read_write);
					region = boostipc::mapped_region(shm, boostipc::read_write);
					data = static_cast<SharedData*>(region.get_address());
				}
			}

		public:
			SharedData* data;

		private:
			boostipc::shared_memory_object shm;
			boostipc::mapped_region region;
		};

		struct GatewayHealthResponse
		{
			bool failing;
			int minResponseTime;
		};
	}  // namespace

	static SharedMemoryManager sharedMemoryManager{Config::databaseInit};

	static std::optional<GatewayHealthResponse> getGatewayHealth(const std::string& url)
	{
		httplib::Client client(url);
		const auto response = client.Get("/payments/service-health");

		if (response && response->status == 200)
		{
			if (const auto docJson = yyjson_read(response->body.c_str(), response->body.length(), 0))
			{
				std::experimental::scope_exit scopeExit([&]() { yyjson_doc_free(docJson); });

				const auto rootJson = yyjson_doc_get_root(docJson);
				const auto failingJson = yyjson_obj_get(rootJson, "failing");
				const auto minResponseTimeJson = yyjson_obj_get(rootJson, "minResponseTime");

				if (failingJson && minResponseTimeJson)
				{
					return GatewayHealthResponse{
						.failing = yyjson_get_bool(failingJson),
						.minResponseTime = yyjson_get_int(minResponseTimeJson),
					};
				}
			}
		}

		return std::nullopt;
	}

	std::jthread GatewayChooserService::start()
	{
		if (!Config::databaseInit)
			return {};
		else
			return std::jthread(handler);
	}

	void GatewayChooserService::handler()
	{
		std::println("GatewayChooserService started.");

		auto lastDefaultCheck = std::chrono::steady_clock::now() - POLL_TIME;
		auto lastFallbackCheck = std::chrono::steady_clock::now() - POLL_TIME;

		std::optional<GatewayHealthResponse> defaultHealth;
		std::optional<GatewayHealthResponse> fallbackHealth;

		while (!SignalHandling::shouldFinish())
		{
			auto currentChoice = static_cast<PaymentGateway>(sharedMemoryManager.data->currentGateway.load());
			const auto now = std::chrono::steady_clock::now();

			if (now - lastDefaultCheck >= POLL_TIME)
			{
				if (const auto newDefaultHealth = getGatewayHealth(Config::processorDefaultUrl))
					defaultHealth = newDefaultHealth.value();

				lastDefaultCheck = now;
			}

			if (now - lastFallbackCheck >= POLL_TIME)
			{
				if (const auto newFallbackHealth = getGatewayHealth(Config::processorFallbackUrl))
					fallbackHealth = newFallbackHealth.value();

				lastFallbackCheck = now;
			}

			PaymentGateway newChoice = currentChoice;

			if (defaultHealth && fallbackHealth)
			{
				if (!defaultHealth->failing && !fallbackHealth->failing)
				{
					if (defaultHealth->minResponseTime > 100 &&
						defaultHealth->minResponseTime > fallbackHealth->minResponseTime * 2)
					{
						newChoice = PaymentGateway::FALLBACK;
					}
					else
						newChoice = PaymentGateway::DEFAULT;
				}
				else if (!defaultHealth->failing && fallbackHealth->failing)
					newChoice = PaymentGateway::DEFAULT;
				else if (defaultHealth->failing && !fallbackHealth->failing)
					newChoice = PaymentGateway::FALLBACK;
				else
					newChoice = PaymentGateway::DEFAULT;
			}
			else if (defaultHealth && !fallbackHealth)
			{
				if (!defaultHealth->failing)
					newChoice = PaymentGateway::DEFAULT;
				else
					newChoice = PaymentGateway::FALLBACK;
			}
			else if (!defaultHealth && fallbackHealth)
			{
				if (!fallbackHealth->failing)
				{
					newChoice =
						currentChoice == PaymentGateway::DEFAULT ? PaymentGateway::DEFAULT : PaymentGateway::FALLBACK;
				}
				else
					newChoice = PaymentGateway::DEFAULT;
			}
			else
				newChoice = PaymentGateway::DEFAULT;

			if (newChoice != currentChoice)
			{
				currentChoice = newChoice;
				sharedMemoryManager.data->currentGateway = static_cast<std::uint8_t>(currentChoice);

				std::println(
					"Gateway switched to: {}", currentChoice == PaymentGateway::DEFAULT ? "DEFAULT" : "FALLBACK");
			}

			if (defaultHealth.has_value())
			{
				std::println("DEFAULT health: failing: {}, minResponseTime: {}", defaultHealth->failing,
					defaultHealth->minResponseTime);
			}

			if (fallbackHealth.has_value())
			{
				std::println("FALLBACK health: failing: {}, minResponseTime: {}", fallbackHealth->failing,
					fallbackHealth->minResponseTime);
			}

			std::println("Current gateway: {}", currentChoice == PaymentGateway::DEFAULT ? "DEFAULT" : "FALLBACK");

			std::fflush(stdout);

			std::this_thread::sleep_for(POLL_TIME);
		}

		std::println("GatewayChooserService stopped.");
	}

	PaymentGateway GatewayChooserService::getGateway()
	{
		return static_cast<PaymentGateway>(sharedMemoryManager.data->currentGateway.load());
	}

	void GatewayChooserService::switchGatewayTo(PaymentGateway gateway)
	{
		sharedMemoryManager.data->currentGateway = std::to_underlying(gateway);
	}
}  // namespace rinhaback::api
