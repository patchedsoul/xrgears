#!/usr/bin/sh

for f in *.hlsl; do
	DEFAULT_OPTS="-V --hlsl-iomap --auto-map-bindings --shift-cbuffer-binding 0 --shift-texture-binding 1 --shift-sampler-binding 2 -D $f"
	glslangValidator -S vert -e VSMain -o ${f%.hlsl}_vs.spv $DEFAULT_OPTS
	glslangValidator -S frag -e PSMain -o ${f%.hlsl}_fs.spv $DEFAULT_OPTS
done
