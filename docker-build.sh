#!/bin/sh
set -e

docker buildx build --progress plain -f src/api/Dockerfile -t asfernandes/rinhaback25:haproxy-mongoose-lmdb-api .
