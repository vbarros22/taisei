#version 330 core

#include "lib/render_context.glslh"
#include "interface/standard.glslh"

UNIFORM(1) float threshold;

void main(void) {
	vec4 clr = texture(tex, texCoord);

	// Wii U debug (temporary): left quarter. Top half shows the threshold
	// uniform (red = 0, green = 1, blue = other), bottom half the alpha read
	// from the reflections buffer (black = 0, white = 1).
	if(texCoordRaw.x < 0.25) {
		if(texCoordRaw.y > 0.5) {
			fragColor = abs(threshold) < 0.01 ? vec4(1, 0, 0, 1) :
				abs(threshold - 1.0) < 0.01 ? vec4(0, 1, 0, 1) : vec4(0, 0, 1, 1);
		} else {
			fragColor = vec4(vec3(clamp(clr.a, 0.0, 1.0)), 1);
		}
		return;
	}

	if(clr.a < threshold) {
		discard;
	}

	fragColor = clr;
}
