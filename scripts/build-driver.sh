#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
xcodebuild -project CXADC.xcodeproj -scheme CXADCDriver -configuration Debug -sdk driverkit -derivedDataPath "$PWD/build/DerivedData" CLANG_MODULE_CACHE_PATH="$PWD/build/ModuleCache.noindex" ALWAYS_SEARCH_USER_PATHS=NO CODE_SIGNING_ALLOWED=NO build "$@"
