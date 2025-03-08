mkdir -p build

# Detect architecture
ARCH=$(uname -m)

# Set linuxdeploy filename based on architecture
if [ "$ARCH" = "x86_64" ]; then
    LINUXDEPLOY_FILENAME=linuxdeploy-x86_64.AppImage
elif [ "$ARCH" = "aarch64" ]; then
    LINUXDEPLOY_FILENAME=linuxdeploy-aarch64.AppImage
else
    echo "Unsupported architecture: $ARCH"
    exit 1
fi

# Download LinuxDeploy if not already downloaded.
if [ ! -f "build/$LINUXDEPLOY_FILENAME" ]; then
    wget -O "build/$LINUXDEPLOY_FILENAME" "https://github.com/linuxdeploy/linuxdeploy/releases/download/2.0.0-alpha-1-20241106/$LINUXDEPLOY_FILENAME"
    if [ $? -ne 0 ]; then
        curl -L -o "build/$LINUXDEPLOY_FILENAME" "https://github.com/linuxdeploy/linuxdeploy/releases/download/2.0.0-alpha-1-20241106/$LINUXDEPLOY_FILENAME"
    fi
fi

# Make LinuxDeploy executable.
chmod +x "build/$LINUXDEPLOY_FILENAME"

# CMake configuration
cmake -B build ../../ \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
    -DCMAKE_POLICY_DEFAULT_CMP0069=NEW \
    -DCLOWNMDEMU_FRONTEND_FREETYPE=ON

# Build the project in parallel
cmake --build build --config Release --parallel $(nproc)

# Prepare AppDir
rm -rf build/AppDir
mkdir -p build/AppDir

# Install into AppDir
DESTDIR=AppDir cmake --build build --target install

# Produce the AppImage with architecture-specific naming
LINUXDEPLOY_OUTPUT_VERSION=v1.2 \
LDAI_UPDATE_INFORMATION="gh-releases-zsync|Clownacy|clownmdemu-frontend|latest|ClownMDEmu-*${ARCH}.AppImage.zsync" \
"build/$LINUXDEPLOY_FILENAME" --appdir build/AppDir --output appimage
