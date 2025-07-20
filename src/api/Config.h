#pragma once

#include <string>
#include <cstdlib>


namespace rinhaback::api
{
	class Config final
	{
	private:
		static std::string readEnv(const char* name, const char* defaultVal)
		{
			const auto val = std::getenv(name);
			return val ? val : defaultVal;
		}

	public:
		Config() = delete;

	public:
		static inline const auto serverWorkers = (unsigned) std::stoi(readEnv("SERVER_WORKERS", "1"));
		static inline const auto serverPollTime = (unsigned) std::stoi(readEnv("SERVER_POLL_TIME", "1"));
		static inline const auto processorWorkers = (unsigned) std::stoi(readEnv("PROCESSOR_WORKERS", "1"));
		static inline const auto database = readEnv("DATABASE", "/data/database");
		static inline const auto databaseSize = (unsigned) std::stoi(readEnv("DATABASE_SIZE", "10485760"));
		static inline const auto databaseInit = readEnv("DATABASE_INIT", "false") == "true";
		static inline const auto listenAddress = readEnv("LISTEN_ADDRESS", "0.0.0.0:8080");
		static inline const auto processorDefaultUrl =
			readEnv("PROCESSOR_DEFAULT_URL", "http://payment-processor-default:8080");
		static inline const auto processorFallbackUrl =
			readEnv("PROCESSOR_FALLBACK_URL", "http://payment-processor-fallback:8080");
	};
}  // namespace rinhaback::api
