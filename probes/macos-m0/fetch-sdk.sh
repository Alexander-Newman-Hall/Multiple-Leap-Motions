#!/bin/bash
# External, official SDK input only. No installer/service execution or redistribution.
set -euo pipefail
[ "$(uname -s)" = Darwin ] && [ "$(uname -m)" = arm64 ] || {
  echo 'BLOCKED: this validation requires native Apple Silicon macOS arm64' >&2
  exit 1
}
mkdir -p work/macos-m0
curl --fail --location --retry 3 --retry-all-errors --retry-delay 5 \
  --connect-timeout 30 --max-time 600 \
  https://s3.eu-west-1.amazonaws.com/downloads.ultraleap.com/software/tracking-software/6.2.0/tracking-software-apple-silicon-6.2.0.pkg \
  -o work/macos-m0/hyperion.pkg
echo '9cad82a6282e322d2acd71f3746e50b472f3ed83bb5f9fa6f54d554f404c5531  work/macos-m0/hyperion.pkg' | shasum -a 256 -c -
pkgutil --check-signature work/macos-m0/hyperion.pkg
pkgutil --expand-full work/macos-m0/hyperion.pkg work/macos-m0/sdk
