#version 330 core

#include "lib/sprite_main.frag.glslh"

void spriteMain(out vec4 fragColor) {
	// Wii U debug (temporary): paint the texture coordinate as colour
	// (red = x, green = y, blue = position inside the quad).
	fragColor = vec4(texCoord.x, texCoord.y, texCoordRaw.x, 1.0);
}
