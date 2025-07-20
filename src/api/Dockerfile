# syntax=docker/dockerfile:1

FROM ubuntu:25.04 AS builder

RUN apt update && \
	apt install -y \
		autoconf \
		cmake \
		curl \
		g++ \
		git \
		ninja-build \
		pkg-config \
		zip && \
	apt-get clean && \
	rm -rf /var/lib/apt/lists/*

COPY . /src

WORKDIR /src

ENV VCPKG_DEFAULT_BINARY_CACHE=/vcpkg-cache

RUN --mount=type=cache,target=$VCPKG_DEFAULT_BINARY_CACHE \
	([ -f ./vcpkg/bootstrap-vcpkg.sh ] || git submodule update --init) && \
	cmake -S . -B /build/Release -DCMAKE_BUILD_TYPE=Release -DCMAKE_VERBOSE_MAKEFILE=ON -G Ninja \
		-DVCPKG_TARGET_TRIPLET=x64-linux-gplusplus-stdcplusplus && \
	cmake --build /build/Release/ && \
	mkdir -p /app/bin && \
	cp /build/Release/out/bin/rinhaback25-haproxy-mongoose-lmdb-api /app/bin/


FROM ubuntu:23.10

WORKDIR /app/bin

COPY --from=builder /app /app

CMD ["/app/bin/rinhaback25-haproxy-mongoose-lmdb-api"]
