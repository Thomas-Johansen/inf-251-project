#version 400
#extension GL_ARB_shading_language_include : require
#include "/model-globals.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

uniform vec2 viewportSize;

in vertexData
{
	vec3 position;
	vec3 normal;
	vec2 texCoord;
} vertices[];

out fragmentData
{
	vec3 position;
	vec3 normal;
	vec2 texCoord;
	noperspective vec3 edgeDistance;
	vec3 tangent; // Pass tangent as an attribute
	vec3 bitangent; // Pass bitangent as an attribute
} fragment;

void main(void)
{
	vec2 p[3];
	vec2 v[3];

	for (int i=0;i<3;i++)
		p[i] = 0.5 * viewportSize *  gl_in[i].gl_Position.xy/gl_in[i].gl_Position.w;

	v[0] = p[2]-p[1];
	v[1] = p[2]-p[0];
	v[2] = p[1]-p[0];

	float area = abs(v[1].x*v[2].y - v[1].y * v[2].x);

	for (int i=0;i<3;i++)
	{
		gl_Position = gl_in[i].gl_Position;
		fragment.position = vertices[i].position;
		fragment.normal = vertices[i].normal;
		fragment.texCoord = vertices[i].texCoord;
		
		vec3 ed = vec3(0.0);
		ed[i] = area / length(v[i]);
		fragment.edgeDistance = ed;

    // Calculate tangent and bitangent
    vec3 p0 = vertices[0].position;
    vec3 p1 = vertices[1].position;
    vec3 p2 = vertices[2].position;

    vec2 uv0 = vertices[0].texCoord;
    vec2 uv1 = vertices[1].texCoord;
    vec2 uv2 = vertices[2].texCoord;
    
    vec3 e0 = p1 - p0;
    vec3 e1 = p2 - p0;
    vec2 dUV0 = uv1 - uv0;
    vec2 dUV1 = uv2 - uv0;

    float f = 1.0 / (dUV0.x * dUV1.y - dUV1.x * dUV0.y);

    fragment.tangent.x = f * (dUV1.y * e0.x - dUV0.y * e1.x);
    fragment.tangent.y = f * (dUV1.y * e0.y - dUV0.y * e1.y);
    fragment.tangent.z = f * (dUV1.y * e0.z - dUV0.y * e1.z);
    fragment.tangent = normalize(fragment.tangent);

    fragment.bitangent.x = f * (-dUV1.x * e0.x + dUV0.x * e1.x);
    fragment.bitangent.y = f * (-dUV1.x * e0.y + dUV0.x * e1.y);
    fragment.bitangent.z = f * (-dUV1.x * e0.z + dUV0.x * e1.z);
    fragment.bitangent = normalize(fragment.bitangent);

		EmitVertex();
	}

	EndPrimitive();
}