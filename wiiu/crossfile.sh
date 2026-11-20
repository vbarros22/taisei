#!/bin/bash
# Prints a Meson cross file for the Wii U (devkitPPC + wut), like
# switch/crossfile.sh. GX2GL_SDK points at the gx2gl SDK: its headers,
# libgl33_gx2_core.a and libcafeglsl.a.

set -eo pipefail

source "$DEVKITPRO/wiiuvars.sh"

: "${GX2GL_SDK:?set GX2GL_SDK to the gx2gl SDK folder}"

function meson_arg_list() {
	while (( "$#" )); do
		echo -n "'$1'";
		if [ $# -gt 1 ]; then
				echo -n ",";
		fi
		shift
	done
}

function bin_path() {
	which "${TOOL_PREFIX}$1"
}

GX2GL_CPPFLAGS="-I$GX2GL_SDK/include"
GX2GL_LIBS="$GX2GL_SDK/lib/libgl33_gx2_core.a $GX2GL_SDK/lib/libcafeglsl.a -lstdc++"

# Revert upstream MSYS2 support mitigation
CPPFLAGS="$(echo "$CPPFLAGS" | sed "s/-I /-I/g")"

cat <<DOCEND
[binaries]
c = '$(bin_path gcc)'
cpp = '$(bin_path g++)'
ar = '$(bin_path gcc-ar)'
strip = '$(bin_path strip)'
pkgconfig = '$PORTLIBS_PREFIX/bin/powerpc-eabi-pkg-config'

[built-in options]
c_args = [$(meson_arg_list $CPPFLAGS $GX2GL_CPPFLAGS $CFLAGS)]
c_link_args = [$(meson_arg_list $LDFLAGS $GX2GL_LIBS $LIBS)]
cpp_args = [$(meson_arg_list $CPPFLAGS $GX2GL_CPPFLAGS $CXXFLAGS)]
cpp_link_args = [$(meson_arg_list $LDFLAGS $GX2GL_LIBS $LIBS)]
werror = false
prefer_static = true
# elf2rpl can't convert the relocations position independent code uses.
b_staticpic = false
b_pie = false

[host_machine]
system = 'wiiu'
cpu_family = 'ppc'
cpu = 'ppc750'
endian = 'big'
DOCEND
