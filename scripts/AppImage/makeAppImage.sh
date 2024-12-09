mkdir build

# Download LinuxDeploy.
if [ ! -f build/linuxdeploy-x86_64.AppImage ]; then
	wget -O build/linuxdeploy-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy/releases/download/2.0.0-alpha-1-20241106/linuxdeploy-x86_64.AppImage
	if [ $? -ne 0 ]; then
		curl -L -o build/linuxdeploy-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy/releases/download/2.0.0-alpha-1-20241106/linuxdeploy-x86_64.AppImage
	fi
fi

# Make LinuxDeploy executable.
chmod +x build/linuxdeploy-x86_64.AppImage

# We want...
# - An optimised Release build.
# - For the program to not be installed to '/usr/local', so that LinuxDeploy detects it.
# - Link-time optimisation, for improved optimisation.
# - To set the CMake policy that normally prevents link-time optimisation.
# - FreeType font rendering, since using a system library means less bloat in the executable.
cmake -B build ../../ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DCMAKE_POLICY_DEFAULT_CMP0069=NEW -DCLOWNMDEMU_FRONTEND_FREETYPE=ON

# Once again specify the Release build, for generators that required it be done this way.
# Build in parallel to speed-up compilation greatly.
cmake --build build --config Release --parallel $(nproc)

# Make a temporary directory to install the built files into.
# Make sure that it is fresh and empty, in case this script was ran before. We don't want old, leftover files.
rm -r build/AppDir
mkdir build/AppDir

# Install into the temporary directory.
DESTDIR=AppDir cmake --build build --target install

# Produce the AppImage, and bundle it with update metadata.
LINUXDEPLOY_OUTPUT_VERSION=v1.0 LDAI_UPDATE_INFORMATION="gh-releases-zsync|Clownacy|clownmdemu|latest|clownmdemu-*x86_64.AppImage.zsync" build/linuxdeploy-x86_64.AppImage --appdir build/AppDir --output appimage
