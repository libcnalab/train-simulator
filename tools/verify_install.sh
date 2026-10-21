#!/usr/bin/env bash

set -euo pipefail

repository_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${1:-"${repository_root}/build"}

if [[ ! -x "${build_dir}/train-simulator" ]]; then
    echo "Train Simulator executable not found in build directory: ${build_dir}" >&2
    echo "Build the project first or pass the build directory as the first argument." >&2
    exit 2
fi

install_root=$(mktemp -d /tmp/train-simulator-install.XXXXXX)
run_root=$(mktemp -d /tmp/train-simulator-installed-run.XXXXXX)

cleanup() {
    rm -rf -- "${install_root}" "${run_root}"
}
trap cleanup EXIT

install_log="${run_root}/cmake-install.log"
if ! cmake --install "${build_dir}" --prefix "${install_root}" --strip \
    >"${install_log}" 2>&1; then
    cat "${install_log}" >&2
    exit 1
fi

installed_binary="${install_root}/bin/train-simulator"
installed_assets="${install_root}/share/train-simulator/assets"
if [[ ! -x "${installed_binary}" ]]; then
    echo "Installed executable is missing: ${installed_binary}" >&2
    exit 1
fi
required_files=(
    "${installed_assets}/config/train-simulator.cfg"
    "${installed_assets}/audio/rolling.wav"
    "${install_root}/lib/libSDL3_image.so.0"
    "${install_root}/share/licenses/train-simulator/TrainSimulator-LICENSE"
    "${install_root}/share/licenses/train-simulator/CNA-LICENSE"
    "${install_root}/share/licenses/train-simulator/SharpRuntime-LICENSE"
    "${install_root}/share/licenses/train-simulator/SDL/SDL3/LICENSE.txt"
    "${install_root}/share/doc/train-simulator/docs/ASSETS.md"
)
for required_file in "${required_files[@]}"; do
    if [[ ! -f "${required_file}" ]]; then
        echo "Required installed file is missing: ${required_file}" >&2
        exit 1
    fi
done

linkage=$(ldd "${installed_binary}")
if grep -Fq "not found" <<<"${linkage}"; then
    echo "Installed executable has an unresolved shared-library dependency." >&2
    printf '%s\n' "${linkage}" >&2
    exit 1
fi
resolved_library() {
    local soname=$1
    awk -v soname="${soname}" '$1 == soname { print $3; exit }' <<<"${linkage}"
}

resolved_sdl=$(resolved_library libSDL3.so.0)
resolved_mixer=$(resolved_library libSDL3_mixer.so.0)
expected_sdl=$(realpath -- "${install_root}/lib/libSDL3.so.0")
expected_mixer=$(realpath -- "${install_root}/lib/libSDL3_mixer.so.0")
# cna-engine needs CNA linked statically into the executable.
if [[ -n "$(resolved_library libcna.so)" ]]; then
    echo "Installed executable links a shared libcna; CNA must be static." >&2
    printf '%s\n' "${linkage}" >&2
    exit 1
fi
if [[ -z "${resolved_sdl}" ]] \
    || [[ $(realpath -- "${resolved_sdl}") != "${expected_sdl}" ]]; then
    echo "Installed executable did not resolve SDL3 from its own prefix." >&2
    printf '%s\n' "${linkage}" >&2
    exit 1
fi
if [[ -z "${resolved_mixer}" ]] \
    || [[ $(realpath -- "${resolved_mixer}") != "${expected_mixer}" ]]; then
    echo "Installed executable did not resolve SDL3_mixer from its own prefix." >&2
    printf '%s\n' "${linkage}" >&2
    exit 1
fi

runtime_output=$(
    cd -- "${run_root}"
    env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
        "${installed_binary}" --frames 2 --quality 0 --no-audio --no-avatars --report-state \
        --config "${run_root}/preferences/train-simulator.cfg" 2>&1
)
printf '%s\n' "${runtime_output}"

if ! grep -Fq "CNA: graphics renderer: OPENGL33" <<<"${runtime_output}"; then
    echo "Installed smoke did not initialize CNA OpenGL 3.3." >&2
    exit 1
fi
if ! grep -Fq "FINAL_STATE" <<<"${runtime_output}"; then
    echo "Installed smoke did not complete a rendered application frame." >&2
    exit 1
fi

echo "Installed Train Simulator passed relocation, linkage, asset, and runtime checks."
