#pragma once

#include <atomic>
#include <chrono>


namespace rinhaback::api
{
	class SignalHandling final
	{
	public:
		SignalHandling() = delete;

	public:
		static void install();

		static bool shouldFinish()
		{
			return finish;
		}

	private:
		static void handler(int)
		{
			finish = true;
		}

	public:
		static inline constexpr auto WAIT_TIME = std::chrono::seconds(2);

	private:
		static inline std::atomic_bool finish{false};
	};
}  // namespace rinhaback::api
