#!/bin/bash
set -e

source libs/env_deploy.sh
source libs/get_source_env.sh
pushd ..

####

if [ ! -d "sing-box" ]; then
  if [ -z "${SING_BOX_REPO_URL:-}" ]; then
    SING_BOX_REPO_URL="https://github.com/SagerNet/sing-box.git"
    echo "SING_BOX_REPO_URL is not set. Using fallback: $SING_BOX_REPO_URL" >&2
  fi
  git clone --no-checkout "$SING_BOX_REPO_URL" sing-box
fi
pushd sing-box
git checkout "$COMMIT_SING_BOX"

popd
popd
