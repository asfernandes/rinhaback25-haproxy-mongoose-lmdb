#pragma once

#include <chrono>
#include <sstream>
#include <string>
#include <cstdint>


namespace rinhaback::api
{
	inline constexpr int HTTP_STATUS_OK = 200;
	inline constexpr int HTTP_STATUS_UNPROCESSABLE_CONTENT = 422;
	inline constexpr int HTTP_STATUS_INTERNAL_SERVER_ERROR = 500;

	inline const std::string HTTP_CONTENT_TYPE_JSON = "application/json";

	using DateTimeMillis = std::chrono::sys_time<std::chrono::milliseconds>;

	inline DateTimeMillis getCurrentDateTime()
	{
		return std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
	}

	inline std::int64_t getCurrentDateTimeAsInt()
	{
		return getCurrentDateTime().time_since_epoch().count();
	}

	inline DateTimeMillis parseDateTime(const std::string& str)
	{
		DateTimeMillis dateTime;

		std::istringstream iss{str};
		std::chrono::from_stream(iss, "%FT%T%Z", dateTime);

		if (iss.fail())
			throw std::invalid_argument("Invalid date time: " + str);

		return dateTime;
	}
}  // namespace rinhaback::api
