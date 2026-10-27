#!/usr/bin/env bash
# A GPU for tests/gpu_test.cpp on every CI row, and SDL3M_REQUIRE_GPU=1 so
# that a missing device fails the test instead of skipping it:
#
#   Linux, Windows  lavapipe (Mesa's Vulkan on the CPU) and the Vulkan loader,
#                   both from xlings; SDL is pointed at that loader and the
#                   loader at lavapipe
#   macOS           the runner's Metal device, through SDL's cocoa video
#                   driver (the offscreen driver makes no Metal views, so
#                   SDL_GPU cannot choose Metal under it)
#
# Writes the environment to $GITHUB_ENV (stdout without it).
set -euo pipefail
out=${GITHUB_ENV:-/dev/stdout}
say() { echo "$1" >> "$out"; echo "gpu: $1"; }

case "$(uname -s)" in
    Darwin)
        say "SDL3M_GPU_VIDEO_DRIVER=cocoa"
        say "SDL3M_REQUIRE_GPU=1"
        exit 0
        ;;
esac

for attempt in 1 2 3; do
    if xlings install mesa-lavapipe vulkan-loader -y -g; then break; fi
    [ "$attempt" = 3 ] && { echo "::error::lavapipe could not be installed"; exit 1; }
    sleep 30
done
root="$HOME/.xlings/data/xpkgs"
icd=$(find "$root" -path '*mesa-lavapipe*' -name 'lvp_icd*.json' | sort | tail -1)
[ -n "$icd" ] || { echo "::error::lavapipe's driver manifest (lvp_icd*.json) is not where xlings installs"; exit 1; }
case "$(uname -s)" in
    Linux) loader=$(find "$root" -path '*vulkan-loader*' -name 'libvulkan.so.1' | sort | tail -1) ;;
    *)
        loader=$(find "$root" -path '*vulkan-loader*' -iname 'vulkan-1.dll' | sort | tail -1)
        icd=$(cygpath -w "$icd")
        loader=$(cygpath -w "$loader")
        ;;
esac
[ -n "$loader" ] || { echo "::error::the Vulkan loader is not where xlings installs"; exit 1; }
if [ "$(uname -s)" = Linux ]; then
    driver=$(dirname "$icd")/../../../lib/libvulkan_lvp.so
    for so in "$loader" "$driver"; do
        if ldd "$so" | grep -q 'not found'; then ldd "$so"; echo "::error::$so has unresolved libraries"; exit 1; fi
    done
fi
say "VK_DRIVER_FILES=$icd"
say "VK_ICD_FILENAMES=$icd"
say "SDL3M_VULKAN_LIBRARY=$loader"
say "SDL3M_REQUIRE_GPU=1"
