#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${ROOT_DIR}/build-win64"

echo "Building BFTools and BFView for Windows (x86_64-w64-mingw32)"

TOOLCHAIN_FILE="${ROOT_DIR}/cmake/toolchain-mingw64.cmake"
if [ ! -f "${TOOLCHAIN_FILE}" ]; then
    echo "Error: Toolchain file ${TOOLCHAIN_FILE} not found!"
    exit 1
fi

cmake -B "${BUILD_DIR}" \
      -S "${ROOT_DIR}" \
      -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_BFVIEW=ON \
      -DTRACY_ENABLE=OFF \
      -DBUILD_STATIC=ON

cmake --build "${BUILD_DIR}" --config Release -j$(nproc)

echo "Build finished successfully!"

RELEASE_DIR="${ROOT_DIR}/release/bin_windows_amd64"
mkdir -p "${RELEASE_DIR}"

echo "Deploying Windows runtime to ${RELEASE_DIR}"

# 1. Copy executables
for exe in BFTools BFView; do
    SRC="${BUILD_DIR}/projects/${exe}/${exe}.exe"
    if [ -f "${SRC}" ]; then
        echo "Deploying ${exe}.exe ..."
        cp -f "${SRC}" "${RELEASE_DIR}/"
    else
        echo "Warning: ${SRC} not found"
    fi
done

if [ -f "${SCRIPT_DIR}/../projects/BFView/resources/BFView.ico" ]; then
    echo "Deploying BFView.ico ..."
    cp -f "${SCRIPT_DIR}/../projects/BFView/resources/BFView.ico" "${RELEASE_DIR}/"
fi

# 2. Locate Qt installation
QT_WIN_DIR=""
for cand in \
    "${HOME}"/Qt/*/mingw_64 \
    /opt/Qt/*/mingw_64 \
    /opt/qt*/mingw_64; do
    if [ -f "${cand}/bin/Qt6Core.dll" ]; then
        QT_WIN_DIR="${cand}"
        break
    fi
done

if [ -z "${QT_WIN_DIR}" ] && [ -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    CMAKE_QT_PATH="$(grep -E 'Qt6(Core)?_DIR:PATH=' "${BUILD_DIR}/CMakeCache.txt" | head -n1 | cut -d= -f2)"
    if [ -n "${CMAKE_QT_PATH}" ]; then
        CAND="$(cd "${CMAKE_QT_PATH}/../../.." 2>/dev/null && pwd)"
        if [ -f "${CAND}/bin/Qt6Core.dll" ]; then
            QT_WIN_DIR="${CAND}"
        fi
    fi
fi

# 3. Deploy Qt DLLs and stock plugins directly to subfolders
PLUGIN_DIRS=(platforms styles imageformats iconengines tls networkinformation)

if [ -n "${QT_WIN_DIR}" ]; then
    echo "Deploying Qt runtime from ${QT_WIN_DIR} ..."

    for dll in \
        Qt6Core.dll \
        Qt6Gui.dll \
        Qt6Widgets.dll \
        Qt6Svg.dll \
        Qt6Network.dll \
        Qt6OpenGL.dll \
        Qt6OpenGLWidgets.dll \
        Qt6PrintSupport.dll \
        Qt6Concurrent.dll \
        Qt6DBus.dll \
        libgcc_s_seh-1.dll \
        libstdc++-6.dll \
        libwinpthread-1.dll; do
        if [ -f "${QT_WIN_DIR}/bin/${dll}" ]; then
            cp -f "${QT_WIN_DIR}/bin/${dll}" "${RELEASE_DIR}/"
        fi
    done

    for plugin_dir in "${PLUGIN_DIRS[@]}"; do
        if [ -d "${QT_WIN_DIR}/plugins/${plugin_dir}" ]; then
            mkdir -p "${RELEASE_DIR}/${plugin_dir}"
            cp -rf "${QT_WIN_DIR}/plugins/${plugin_dir}/." \
                "${RELEASE_DIR}/${plugin_dir}/"
        fi
    done
else
    echo "Warning: Windows Qt installation not found. Stock Qt plugins skipped."
fi

# 4. Deploy project-built DLLs into appropriate subfolders (never duplicate in root)
find "${BUILD_DIR}" -type f -iname '*.dll' \
    -path '*/projects/*' -print0 |
while IFS= read -r -d '' dll; do
    filename="$(basename "${dll}")"
    lower_filename="${filename,,}"

    target_subfolder=""

    # Route based on plugin type
    if [[ "${lower_filename}" =~ (style|modernwindows) ]]; then
        target_subfolder="styles"
    elif [[ "${lower_filename}" =~ (svgicon|iconengine) ]]; then
        target_subfolder="iconengines"
    elif [[ "${lower_filename}" =~ (imageformat|qjpeg|qgif|qsvg|qico) ]]; then
        target_subfolder="imageformats"
    elif [[ "${lower_filename}" =~ (qwindows|platform) ]]; then
        target_subfolder="platforms"
    fi

    if [ -n "${target_subfolder}" ]; then
        echo "Deploying plugin ${filename} -> ${target_subfolder}/"
        mkdir -p "${RELEASE_DIR}/${target_subfolder}"
        cp -f "${dll}" "${RELEASE_DIR}/${target_subfolder}/"
        # Ensure it doesn't also linger in the root
        rm -f "${RELEASE_DIR}/${filename}"
    else
        # Standard linked library (non-plugin)
        cp -u "${dll}" "${RELEASE_DIR}/"
    fi
done

# 5. Qt plugin configuration
cat > "${RELEASE_DIR}/qt.conf" << 'EOF'
[Paths]
Prefix = .
Plugins = .
EOF
