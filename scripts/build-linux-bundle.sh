#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-linux"
DIST_DIR="${ROOT_DIR}/dist"
BUNDLE_DIR="${DIST_DIR}/BFView-linux-x86_64"

echo "Building BFTools and BFView for Linux"
cmake -B "${BUILD_DIR}" -S "${ROOT_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_BFVIEW=ON \
    -DBUILD_STATIC=OFF \
    -DTRACY_ENABLE=OFF \
    -DBUILD_TESTS=ON

cmake --build "${BUILD_DIR}" --config Release -j"$(nproc)"

echo "Creating portable directory bundle at ${BUNDLE_DIR}"
rm -rf "${BUNDLE_DIR}"
mkdir -p "${BUNDLE_DIR}/lib"
mkdir -p "${BUNDLE_DIR}/plugins"

if [ -f "${BUILD_DIR}/projects/BFView/BFView" ]; then
    cp -f "${BUILD_DIR}/projects/BFView/BFView" "${BUNDLE_DIR}/"
    chmod +x "${BUNDLE_DIR}/BFView"
fi

if [ -f "${BUILD_DIR}/projects/BFTools/BFTools" ]; then
    cp -f "${BUILD_DIR}/projects/BFTools/BFTools" "${BUNDLE_DIR}/"
    chmod +x "${BUNDLE_DIR}/BFTools"
fi

if [ -d "${BUILD_DIR}/projects/BFView/projects" ]; then
    echo "Deploying projects directory to ${BUNDLE_DIR}/projects ..."
    cp -rf "${BUILD_DIR}/projects/BFView/projects" "${BUNDLE_DIR}/"
elif [ -d "${ROOT_DIR}/projects/BFView/projects" ]; then
    echo "Deploying projects directory from source to ${BUNDLE_DIR}/projects ..."
    cp -rf "${ROOT_DIR}/projects/BFView/projects" "${BUNDLE_DIR}/"
fi

QT_LIBS_DIR=""
QT_PLUGINS_DIR=""

if command -v qmake6 >/dev/null 2>&1; then
    QT_LIBS_DIR="$(qmake6 -query QT_INSTALL_LIBS 2>/dev/null || true)"
    QT_PLUGINS_DIR="$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
fi

if [ -z "${QT_LIBS_DIR}" ] || [ ! -d "${QT_LIBS_DIR}" ]; then
    for cand in "/usr/lib" "/usr/lib64" "/usr/lib/x86_64-linux-gnu"; do
        if [ -f "${cand}/libQt6Core.so.6" ]; then
            QT_LIBS_DIR="${cand}"
            break
        fi
    done
fi

if [ -z "${QT_PLUGINS_DIR}" ] || [ ! -d "${QT_PLUGINS_DIR}" ]; then
    for cand in "/usr/lib/qt6/plugins" "/usr/lib64/qt6/plugins" "/usr/lib/x86_64-linux-gnu/qt6/plugins"; do
        if [ -d "${cand}/platforms" ]; then
            QT_PLUGINS_DIR="${cand}"
            break
        fi
    done
fi

echo "Using Qt Libs: ${QT_LIBS_DIR}"
echo "Using Qt Plugins: ${QT_PLUGINS_DIR}"

echo "Bundling Qt6 shared libraries..."
copy_lib_pattern() {
    local src_dir="$1"
    local pattern="$2"
    local dst_dir="$3"
    find "${src_dir}" -maxdepth 1 -name "${pattern}" 2>/dev/null | while read -r f; do
        if [ -L "$f" ]; then
            local link_target
            link_target="$(readlink "$f")"
            local base_name
            base_name="$(basename "$f")"
            if [ ! -e "${dst_dir}/${base_name}" ]; then
                ln -sf "${link_target}" "${dst_dir}/${base_name}"
            fi
        elif [ -f "$f" ]; then
            cp -n "$f" "${dst_dir}/" 2>/dev/null || true
        fi
    done
}

QT_MODULES=("Core" "Gui" "Widgets" "Svg" "DBus" "XcbQpa" "WaylandClient" "WaylandEglClientHwIntegration" "OpenGL" "OpenGLWidgets" "Quick" "Qml" "QmlModels" "QmlMeta" "QmlWorkerScript" "Network" "QuickControls2" "QuickTemplates2" "Xml" "Concurrent")
for mod in "${QT_MODULES[@]}"; do
    copy_lib_pattern "${QT_LIBS_DIR}" "libQt6${mod}.so*" "${BUNDLE_DIR}/lib"
done

echo "Bundling KF6 style and integration libraries..."
KF6_MODULES=("KF6WindowSystem" "KF6Style" "KF6IconThemes" "KF6ColorScheme" "KF6ConfigGui" "KF6ConfigCore" "KF6GuiAddons" "KF6WidgetsAddons" "KF6Archive" "KF6BreezeIcons" "KF6I18n" "KF6CoreAddons" "KF6Crash")
for kmod in "${KF6_MODULES[@]}"; do
    copy_lib_pattern "${QT_LIBS_DIR}" "lib${kmod}.so*" "${BUNDLE_DIR}/lib"
done

echo "Bundling Qt6 plugins..."
PLUGIN_DIRS=("platforms" "imageformats" "iconengines" "xcbglintegrations" "wayland-graphics-integration-client" "wayland-shell-integration" "styles" "platformthemes")
for pdir in "${PLUGIN_DIRS[@]}"; do
    if [ -d "${QT_PLUGINS_DIR}/${pdir}" ]; then
        mkdir -p "${BUNDLE_DIR}/plugins/${pdir}"
        find "${QT_PLUGINS_DIR}/${pdir}" -maxdepth 1 -name "*.so" 2>/dev/null | while read -r pf; do
            pname="$(basename "$pf")"
            # Skip third-party plugins in imageformats (e.g. kimg_*)
            if [ "${pdir}" = "imageformats" ] && [[ "${pname}" == kimg_* ]]; then
                continue
            fi
            cp -f "$pf" "${BUNDLE_DIR}/plugins/${pdir}/"
        done
    fi
done

echo "Bundling Qt support runtime libraries..."
SUPPORT_LIBS=(
    "libicudata.so*" "libicui18n.so*" "libicuuc.so*"
    "libpcre2-16.so*" "libdouble-conversion.so*" "libb2.so*" "libmd4c.so*"
    "libduktape.so*"
    "libxcb-cursor.so*" "libxcb-icccm.so*" "libxcb-image.so*" "libxcb-keysyms.so*"
    "libxcb-render-util.so*" "libxcb-shape.so*" "libxcb-sync.so*" "libxcb-xfixes.so*"
    "libxcb-xkb.so*" "libxkbcommon.so*" "libxkbcommon-x11.so*"
)
for pat in "${SUPPORT_LIBS[@]}"; do
    copy_lib_pattern "${QT_LIBS_DIR}" "${pat}" "${BUNDLE_DIR}/lib"
done

# 7. Create qt.conf
cat << 'EOF' > "${BUNDLE_DIR}/qt.conf"
[Paths]
Prefix = .
Libraries = lib
Plugins = plugins
EOF

# 8. Create Launcher Script (BFView.sh)
cat << 'EOF' > "${BUNDLE_DIR}/BFView.sh"
#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export QT_PLUGIN_PATH="${SCRIPT_DIR}/plugins"
exec "${SCRIPT_DIR}/BFView" "$@"
EOF
chmod +x "${BUNDLE_DIR}/BFView.sh"

# 9. Configure RPATH with patchelf if available
if command -v patchelf >/dev/null 2>&1; then
    echo "Patching RPATH with patchelf..."
    patchelf --set-rpath '$ORIGIN/lib' "${BUNDLE_DIR}/BFView"
    if [ -f "${BUNDLE_DIR}/BFTools" ]; then
        patchelf --set-rpath '$ORIGIN/lib' "${BUNDLE_DIR}/BFTools"
    fi

    # Also patch all plugin .so files to locate libraries in lib/
    find "${BUNDLE_DIR}/plugins" -type f -name "*.so" 2>/dev/null | while read -r plugf; do
        patchelf --set-rpath '$ORIGIN/../../lib:$ORIGIN/lib' "$plugf" 2>/dev/null || true
    done
fi

echo "Creating archive ${DIST_DIR}/BFView-linux-x86_64.tar.gz ..."
mkdir -p "${DIST_DIR}"
tar -czf "${DIST_DIR}/BFView-linux-x86_64.tar.gz" -C "${DIST_DIR}" "BFView-linux-x86_64"

echo "Linux bundle built successfully at ${DIST_DIR}/BFView-linux-x86_64.tar.gz"
