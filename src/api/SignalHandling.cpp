#include "./SignalHandling.h"
#include <csignal>


namespace rinhaback::api
{
	void SignalHandling::install()
	{
		std::signal(SIGINT, SignalHandling::handler);
		std::signal(SIGTERM, SignalHandling::handler);
	}
}  // namespace rinhaback::api
