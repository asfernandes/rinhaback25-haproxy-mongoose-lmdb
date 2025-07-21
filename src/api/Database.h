#pragma once

#include <array>
#include <format>
#include <print>
#include <utility>
#include <cstdint>
#include "lmdb.h"

#ifndef NDEBUG
#include <source_location>
#endif


namespace rinhaback::api
{
	enum class PaymentGateway : std::uint8_t
	{
		DEFAULT = 0,
		FALLBACK = 1,
		SIZE = 2
	};

	using CorrelationId = std::array<char, 36>;

	inline void checkMdbError(int rc
#ifndef NDEBUG
		,
		const std::source_location location = std::source_location::current()
#endif
	)
	{
		if (rc != 0)
		{
			std::string msg =
#ifndef NDEBUG
				std::format("MDB error: {} in {} at line {}", rc, location.file_name(), location.line());
#else
				std::format("MDB error: {}", rc);
#endif
			std::println(stderr, "{}", msg);
			throw std::runtime_error(msg);
		}
	}

	class Connection final
	{
	public:
		explicit Connection();
		~Connection();

		Connection(const Connection&) = delete;
		Connection& operator=(const Connection&) = delete;

	public:
		MDB_env* env;
		std::array<MDB_dbi, std::to_underlying(PaymentGateway::SIZE)> dbis;
	};

	class Transaction final
	{
	public:
		explicit Transaction(Connection& connection, int flags)
			: connection(connection),
			  flags(flags)
		{
			checkMdbError(mdb_txn_begin(connection.env, nullptr, flags, &txn));
		}

		~Transaction()
		{
			if (flags & MDB_RDONLY)
				mdb_txn_abort(txn);
			else
				mdb_txn_commit(txn);
		}

		Transaction(const Transaction&) = delete;
		Transaction& operator=(const Transaction&) = delete;

	public:
		Connection& connection;
		MDB_txn* txn;
		const int flags;
	};

	inline Connection& getConnection()
	{
		static Connection connection;
		return connection;
	}
}  // namespace rinhaback::api
