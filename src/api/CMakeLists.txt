set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(rinhaback25-haproxy-mongoose-lmdb-api CXX)

file(GLOB_RECURSE SRC
	"*.h"
	"*.cpp"
)

find_package(httplib REQUIRED)
find_package(unofficial-lmdb REQUIRED)
find_package(mimalloc REQUIRED)
find_package(unofficial-mongoose REQUIRED)
find_package(yyjson REQUIRED)


add_library(${PROJECT_NAME}-lib
	${SRC}
	${PROTO_FILES}
)

target_link_libraries(${PROJECT_NAME}-lib
	PUBLIC
		httplib::httplib
		unofficial::lmdb::lmdb
		mimalloc-static
		unofficial::mongoose::mongoose
		yyjson::yyjson
)


add_executable(${PROJECT_NAME}
	${SRC}
)

target_link_libraries(${PROJECT_NAME}
	PUBLIC ${PROJECT_NAME}-lib
)

target_link_options(${PROJECT_NAME}
	PRIVATE
		-static-libgcc
		-static-libstdc++
)
