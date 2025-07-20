#include "./PaymentRepository.h"
#include "./Config.h"
#include "./Database.h"
#include "./Util.h"
#include <string>
#include <utility>
#include <cassert>
#include <cstring>


namespace rinhaback::api
{
	void PaymentRepository::postPayment(double amount, const CorrelationId& correlationId, DateTimeMillis requestedAt)
	{
		Connection& connection = getConnection();

		PaymentKey key;
		key.dateTime = requestedAt.time_since_epoch().count();

		PaymentData data{.amount = amount, .correlationId = correlationId};

		Transaction transaction(connection, 0);

		MDB_val mdbKey(sizeof(key), &key);
		MDB_val mdbData(sizeof(data), &data);
		checkMdbError(mdb_put(transaction.txn, connection.dbis[std::to_underlying(gateway)], &mdbKey, &mdbData, 0),
			__FILE__, __LINE__);
	}

	PaymentRepository::PaymentsGatewaySummaryResponse PaymentRepository::getPaymentsSummary(
		Transaction& transaction, std::optional<std::int64_t> from, std::optional<std::int64_t> to)
	{
		Connection& connection = transaction.connection;

		PaymentsGatewaySummaryResponse response = {
			.totalRequests = 0,
			.totalAmount = 0.0,
		};

		PaymentKey initialKey{.dateTime = from.value_or(0)};

		MDB_val mdbKey(sizeof(initialKey), &initialKey);
		MDB_val mdbData;

		MDB_cursor* cursor;
		checkMdbError(mdb_cursor_open(transaction.txn, connection.dbis[std::to_underlying(gateway)], &cursor), __FILE__,
			__LINE__);

		int rc = mdb_cursor_get(cursor, &mdbKey, &mdbData, (from ? MDB_SET_RANGE : MDB_FIRST));

		if (rc != 0)
		{
			mdb_cursor_close(cursor);

			if (rc != MDB_NOTFOUND)
				checkMdbError(rc, __FILE__, __LINE__);

			return response;
		}

		while (rc == 0)
		{
			const auto* key = static_cast<const PaymentKey*>(mdbKey.mv_data);
			const auto* data = static_cast<const PaymentData*>(mdbData.mv_data);

			if (to.has_value() && key->dateTime > to.value())
			{
				rc = MDB_NOTFOUND;
				break;
			}

			++response.totalRequests;
			response.totalAmount += data->amount;

			rc = mdb_cursor_get(cursor, &mdbKey, &mdbData, MDB_NEXT);
		}

		mdb_cursor_close(cursor);

		if (rc != MDB_NOTFOUND)
			checkMdbError(rc, __FILE__, __LINE__);

		return response;
	};

	void PaymentRepository::purge()
	{
		Connection& connection = getConnection();
		Transaction transaction(connection, 0);

		checkMdbError(mdb_drop(transaction.txn, connection.dbis[std::to_underlying(gateway)], 0), __FILE__, __LINE__);
	}
}  // namespace rinhaback::api
