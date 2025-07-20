#pragma once

#include <array>
#include <print>
#include <utility>
#include <cstdint>
#include "lmdb.h"


namespace rinhaback::api
{
	enum class PaymentGateway : std::uint8_t
	{
		DEFAULT = 0,
		FALLBACK = 1,
		SIZE = 2
	};

	using CorrelationId = std::array<char, 36>;

	inline void checkMdbError(int rc, const char* file, unsigned line)
	{
		if (rc != 0)
		{
			std::println(stderr, "MDB error: {} in {} at line {}", rc, file, line);
			throw std::runtime_error("MDB error: " + std::to_string(rc));
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
			checkMdbError(mdb_txn_begin(connection.env, nullptr, flags, &txn), __FILE__, __LINE__);
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
