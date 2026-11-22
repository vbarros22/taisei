#!/usr/bin/env bash
# Compiles every Taisei shader object the way the Wii U port does:
# Taisei's #include processing, gx2gl's rewriting, then CafeGLSL.
# usage: run.sh <taisei shader dir> <gx2gl checkout> <glslcompiler> <outdir>
set -u
SHADERS="$1"; GX2GL="$2"; GLSLC="$3"; OUT="$4"
HERE="$(cd "${BASH_SOURCE%/*}" && pwd)"
mkdir -p "$OUT/stub" "$OUT/failed"
cp "$GX2GL/src/core/gx2gl_shader_compat.h" "$GX2GL/src/core/gx2gl_shader_compat.cpp" "$OUT/stub/"
cp "$HERE/stub/gl_context.h" "$OUT/stub/"
g++ -std=c++17 -O1 -I"$OUT/stub" -I"$GX2GL/include" "$HERE/lower.cpp" "$OUT/stub/gx2gl_shader_compat.cpp" -o "$OUT/lower" || exit 1

ok=0; fail=0
: > "$OUT/summary.txt"
for f in "$SHADERS"/*.vert.glsl "$SHADERS"/*.frag.glsl; do
	name="$(basename "$f")"
	case "$name" in *.vert.glsl) stage=VERT; kind=vert; flag=-vs;; *) stage=FRAG; kind=frag; flag=-ps;; esac
	python3 "$HERE/preprocess.py" "$SHADERS" "$name" "$stage" > "$OUT/$name.pp" || { echo "PREPROCESS $name" >> "$OUT/summary.txt"; fail=$((fail+1)); continue; }
	if ! "$OUT/lower" "$kind" "$OUT/$name.pp" "$OUT/$name.cafe" 2> "$OUT/$name.log"; then
		echo "GX2GL $name: $(head -c 300 "$OUT/$name.log")" >> "$OUT/summary.txt"
		cp "$OUT/$name.pp" "$OUT/$name.log" "$OUT/failed/"; fail=$((fail+1)); continue
	fi
	if ! timeout 60 "$GLSLC" $flag "$OUT/$name.cafe" -o "$OUT/$name.gsh" > "$OUT/$name.log" 2>&1; then
		echo "CAFEGLSL $name: $(grep -m3 -i 'error' "$OUT/$name.log" | tr '\n' ' ' | head -c 400)" >> "$OUT/summary.txt"
		cp "$OUT/$name.pp" "$OUT/$name.cafe" "$OUT/$name.log" "$OUT/failed/"; fail=$((fail+1)); continue
	fi
	ok=$((ok+1))
done
echo "compiled: $ok, failed: $fail" | tee -a "$OUT/summary.txt"
