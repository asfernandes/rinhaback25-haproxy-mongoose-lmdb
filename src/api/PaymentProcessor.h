#pragma once

#include "./PaymentService.h"
#include "./PendingPaymentsQueue.h"
#include <memory>
#include <thread>


namespace rinhaback::api
{
	class PaymentProcessor final
	{
	public:
		static std::jthread start(
			std::shared_ptr<PendingPaymentsQueue> pendingPaymentsQueue, std::shared_ptr<PaymentService> paymentService);

	private:
		void handler();
		void processPayment(const PendingPaymentsQueue::Payment& payment);

	private:
		std::shared_ptr<PendingPaymentsQueue> pendingPaymentsQueue;
		std::shared_ptr<PaymentService> paymentService;
	};
}  // namespace rinhaback::api
