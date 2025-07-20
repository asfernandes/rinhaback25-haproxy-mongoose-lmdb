#pragma once

#include "./Database.h"
#include <chrono>
#include <thread>


namespace rinhaback::api
{
	class GatewayChooserService final
	{
	public:
		GatewayChooserService() = delete;

	public:
		static std::jthread start();
		static PaymentGateway getGateway();
		static void switchGatewayTo(PaymentGateway gateway);

	private:
		static void handler();

	private:
		static inline constexpr std::chrono::milliseconds POLL_TIME{5010};
	};
}  // namespace rinhaback::api
