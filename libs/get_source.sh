#!/bin/bash
set -e

source libs/env_deploy.sh
source libs/get_source_env.sh
pushd ..

####

if [ ! -d "sing-box" ]; then
  if [ -z "${SING_BOX_REPO_URL:-}" ]; then
    echo "Missing local sing-box source and SING_BOX_REPO_URL is not set." >&2
    echo "Provide your fork URL in SING_BOX_REPO_URL or place sing-box next to this repository." >&2
    exit 1
  fi
  git clone --no-checkout "$SING_BOX_REPO_URL" sing-box
fi
pushd sing-box
git checkout "$COMMIT_SING_BOX"

popd
popd
