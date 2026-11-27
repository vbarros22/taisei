#version 330 core

#include "lib/sprite_main.frag.glslh"

void spriteMain(out vec4 fragColor) {
	vec4 t = texture(tex, texCoord);
	fragColor = color * t * customParams.r;

	// Wii U debug (temporary): red = texel with alpha 0 but colour
	// (not premultiplied), blue = texture coordinate outside the sprite.
	if(t.a < 0.004 && max(t.r, max(t.g, t.b)) > 0.02) {
		fragColor = vec4(0.6, 0.0, 0.0, 0.6);
	}

	if(any(lessThan(texCoordRaw, vec2(-0.01))) || any(greaterThan(texCoordRaw, vec2(1.01)))) {
		fragColor = vec4(0.0, 0.0, 0.6, 0.6);
	}
}
