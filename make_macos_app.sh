#!/bin/bash

make

# copy these dynamic libs and inter-deps over to be bundled, change perms
cp /usr/local/lib/libftd3xx.dylib .
cp /usr/local/lib/libsfml-audio.2.6.dylib /usr/local/lib/libsfml-graphics.2.6.dylib /usr/local/lib/libsfml-system.2.6.dylib /usr/local/lib/libsfml-window.2.6.dylib .
chmod 777 *.dylib
chmod +x xx3dsfml

# edit binary to have ftd3xx and sfml relatively linked (can check with otool -L xx3dsfml to see current dylib paths)
install_name_tool -change libftd3xx.dylib  @executable_path/libftd3xx.dylib  xx3dsfml
install_name_tool -change @rpath/libsfml-audio.2.6.dylib @executable_path/libsfml-audio.2.6.dylib xx3dsfml
install_name_tool -change @rpath/libsfml-graphics.2.6.dylib @executable_path/libsfml-graphics.2.6.dylib xx3dsfml
install_name_tool -change @rpath/libsfml-system.2.6.dylib @executable_path/libsfml-system.2.6.dylib xx3dsfml
install_name_tool -change @rpath/libsfml-window.2.6.dylib @executable_path/libsfml-window.2.6.dylib xx3dsfml

# adjust some of the dylibs to internally reference these relative dylibs as well (can also check with otool -L <dylib>)
install_name_tool -change @rpath/libsfml-system.2.6.dylib @executable_path/libsfml-system.2.6.dylib ./libsfml-audio.2.6.dylib
install_name_tool -change @rpath/libsfml-window.2.6.dylib @executable_path/libsfml-window.2.6.dylib ./libsfml-graphics.2.6.dylib
install_name_tool -change @rpath/libsfml-system.2.6.dylib @executable_path/libsfml-system.2.6.dylib ./libsfml-graphics.2.6.dylib
install_name_tool -change @rpath/libsfml-system.2.6.dylib @executable_path/libsfml-system.2.6.dylib ./libsfml-window.2.6.dylib

# make Info.plist with version from configure.ac
VERSION=v0.0.1


# Build xx3dsfml bundle

cat > Info.plist <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>xx3dsfml</string>
    <key>CFBundleIconFile</key>
	<string>AppIcon</string>
	<key>CFBundleIconName</key>
	<string>AppIcon</string>
	<key>CFBundleIdentifier</key>
	<string>com.catwashere.xx3dsfml</string>
	<key>CFBundleVersion</key>
	<string>${VERSION}</string>
	<key>CFBundleDisplayName</key>
	<string>xx3dsfml</string>
	<key>LSRequiresIPhoneOS</key>
	<string>false</string>
	<key>NSHighResolutionCapable</key>
	<string>true</string>
</dict>
</plist>
EOF

# Create App Icon
mkdir -p AppIcon.iconset
cp icons/* AppIcon.iconset
iconutil -c icns AppIcon.iconset -o AppIcon.icns
rm -rf AppIcon.iconset

# Create App Bundle
mkdir -p xx3dsfml.app/Contents/MacOS xx3dsfml.app/Contents/Resources
mv AppIcon.icns xx3dsfml.app/Contents/Resources
mv xx3dsfml *.dylib xx3dsfml.app/Contents/MacOS
mv Info.plist xx3dsfml.app/Contents
