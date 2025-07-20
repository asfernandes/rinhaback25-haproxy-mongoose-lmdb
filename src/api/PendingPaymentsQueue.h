#pragma once

#include "./SignalHandling.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <cassert>
#include <cstdint>


namespace rinhaback::api
{
	class PendingPaymentsQueue final
	{
	public:
		struct Payment
		{
			double amount;
			CorrelationId correlationId;
		};

	public:
		void enqueue(const Payment& payment)
		{
			{  // scope
				std::unique_lock lock(mutex);
				queue.push(payment);
			}

			condVar.notify_one();
		}

		std::optional<Payment> dequeue()
		{
			do
			{
				std::unique_lock lock(mutex);

				if (!condVar.wait_for(lock, SignalHandling::WAIT_TIME, [&] { return !queue.empty(); }))
				{
					if (SignalHandling::shouldFinish())
						return std::nullopt;

					continue;
				}

				assert(!queue.empty());

				Payment payment = queue.front();
				queue.pop();

				return payment;
			} while (true);
		}

		void purge()
		{
			std::unique_lock lock(mutex);
			queue = {};
		}

	private:
		std::mutex mutex;
		std::condition_variable condVar;
		std::queue<Payment> queue;
	};
}  // namespace rinhaback::api
