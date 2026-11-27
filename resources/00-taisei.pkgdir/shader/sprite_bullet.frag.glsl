#version 330 core

#include "lib/sprite_main.frag.glslh"

#include "lib/util.glslh"

void spriteMain(out vec4 fragColor) {
	// Wii U debug (temporary): paint the texture coordinate as colour.
	fragColor = vec4(texCoord.x, texCoord.y, texCoordRaw.x, 1.0);
}
