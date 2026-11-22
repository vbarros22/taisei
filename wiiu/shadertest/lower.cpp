// Runs gx2gl's source rewriting for CafeGLSL (gx2gl_shader_compat.cpp) on a
// shader, as glCompileShader does on the console.
// usage: lower <vert|frag> <in> <out>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include "gx2gl_shader_compat.h"

int main(int argc, char **argv) {
	if(argc != 4) return 2;
	std::ifstream in(argv[2]);
	std::stringstream ss;
	ss << in.rdbuf();
	char log[2048] = { 0 };
	GLenum type = !strcmp(argv[1], "vert") ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
	char *out = gx2gl_prepare_shader_source_for_cafeglsl_ex(ss.str().c_str(), type, log, sizeof(log));
	if(!out) {
		fprintf(stderr, "gx2gl: %s\n", log);
		return 1;
	}
	FILE *f = fopen(argv[3], "w");
	fputs(out, f);
	fclose(f);
	return 0;
}
