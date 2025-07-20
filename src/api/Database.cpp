#include "./Database.h"
#include "./Config.h"
#include "./Util.h"
#include <bit>
#include <filesystem>
#include <thread>
#include <cstring>

namespace stdfs = std::filesystem;


namespace rinhaback::api
{
	Connection::Connection()
	{
		if (Config::databaseInit)
		{
			if (stdfs::exists(Config::database))
			{
				stdfs::remove(stdfs::path(Config::database).append("data.mdb"));
				stdfs::remove(stdfs::path(Config::database).append("lock.mdb"));
			}
			else
				stdfs::create_directories(Config::database);
		}
		else
			std::this_thread::sleep_for(std::chrono::seconds(2));  // FIXME:

		const int endiannessFlags = std::endian::native == std::endian::little ? (MDB_REVERSEKEY | MDB_REVERSEDUP) : 0;

		checkMdbError(mdb_env_create(&env), __FILE__, __LINE__);
		checkMdbError(mdb_env_set_mapsize(env, Config::databaseSize), __FILE__, __LINE__);
		checkMdbError(mdb_env_set_maxdbs(env, std::to_underlying(PaymentGateway::SIZE)), __FILE__, __LINE__);
		checkMdbError(mdb_env_open(env, Config::database.c_str(),
						  MDB_WRITEMAP | MDB_NOMETASYNC | MDB_NOSYNC | MDB_NOTLS | MDB_NOMEMINIT |
							  (Config::databaseInit ? MDB_CREATE : 0),
						  0664),
			__FILE__, __LINE__);

		Transaction transaction(*this, 0);

		checkMdbError(
			mdb_dbi_open(transaction.txn, "default", MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED | endiannessFlags,
				&dbis[std::to_underlying(PaymentGateway::DEFAULT)]),
			__FILE__, __LINE__);

		checkMdbError(
			mdb_dbi_open(transaction.txn, "fallback", MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED | endiannessFlags,
				&dbis[std::to_underlying(PaymentGateway::FALLBACK)]),
			__FILE__, __LINE__);
	}

	Connection::~Connection()
	{
		for (auto dbi : dbis)
		{
			if (dbi)
				mdb_dbi_close(env, dbi);
		}

		mdb_env_close(env);
	}
}  // namespace rinhaback::api
