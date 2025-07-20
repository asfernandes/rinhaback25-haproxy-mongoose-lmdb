#!/bin/sh
set -e

./docker-build.sh
docker push asfernandes/rinhaback25:haproxy-mongoose-lmdb-api
