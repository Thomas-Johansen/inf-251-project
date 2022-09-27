#version 400
#extension GL_ARB_shading_language_include : require
#include "/model-globals.glsl"

uniform mat4 modelViewProjectionMatrix;

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

out vertexData
{
	vec3 position;
	vec3 normal;
	vec2 texCoord;
} vertex;

void main()
{
	vec4 pos = modelViewProjectionMatrix*vec4(position,1.0);

	vertex.position = position; 
	vertex.normal = normal;
	vertex.texCoord = texCoord;	
	
	gl_Position = pos;
}