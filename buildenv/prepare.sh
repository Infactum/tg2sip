#!/bin/bash

DOCKER_IMAGE=infactum/tg2sip-builder

for DOCKER_TAG in bionic centos6 centos7
do
    docker build . -f Dockerfile."$DOCKER_TAG" -t "$DOCKER_IMAGE:$DOCKER_TAG"
    docker push "$DOCKER_IMAGE:$DOCKER_TAG"
done
