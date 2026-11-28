#version 330 core

#include "lib/render_context.glslh"
#include "interface/standard.glslh"

UNIFORM(1) float threshold;

void main(void) {
	vec4 clr = texture(tex, texCoord);

	float th = threshold;
	// Wii U debug (temporary): threshold > 1.5 skips the left half.
	if(th > 1.5) {
		if(texCoord.x < 0.5) discard;
		th -= 1.0;
	}

	if(clr.a < th) {
		discard;
	}

	fragColor = clr;
}
