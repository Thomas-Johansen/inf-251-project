#version 400
#extension GL_ARB_shading_language_include : require
#include "/model-globals.glsl"

uniform vec3 worldCameraPosition;
uniform vec3 worldLightPosition;
uniform vec3 diffuseColor;
uniform sampler2D diffuseTexture;
uniform vec4 wireframeLineColor;

//New imports
uniform vec3 ambientColor;
uniform vec3 specularColor;
uniform float shininess;
uniform bool hasDiffuseTexture;

//Light intensity
uniform vec3 worldLightIntensity;
uniform vec3 ambientLightIntensity;

//Blinn-Phong spesific
uniform bool ambientEnabled;
uniform bool diffuseEnabled;
uniform bool specularEnabled;
uniform float shininessMultiplier;

//Menu option general
uniform int shaderMenu;

//Assignmet 2 spesific
uniform bool hasAmbientTexture;
uniform bool hasSpecularTexture;
uniform sampler2D ambientTexture;
uniform sampler2D specularTexture;

uniform int normalMenu;
uniform sampler2D objectNormals; // Object space normal map
uniform mat3 normalMatrix; // Normal matrix for transforming object space normals to world space


in fragmentData
{
	vec3 position;
	vec3 normal;
	vec2 texCoord;
	noperspective vec3 edgeDistance;
} fragment;

out vec4 fragColor;

void main()
{
	vec4 result = vec4(0.5,0.5,0.5,1.0);

	//Normal Mapping code
	vec3 normal = normalize(fragment.normal);
	if (normalMenu == 1)
	{
		// Sample the object space normal map
		vec3 objectSpaceNormal = 2.0 * texture(objectNormals, fragment.texCoord).rgb - 1.0;
		// Transform object space normal to world space
		normal = normalize(normalMatrix * objectSpaceNormal);
	}


	//Shading Code
	if (shaderMenu == 0)
	{
		float smallestDistance = min(min(fragment.edgeDistance[0],fragment.edgeDistance[1]),fragment.edgeDistance[2]);
		float edgeIntensity = exp2(-1.0*smallestDistance*smallestDistance);
		result.rgb = mix(result.rgb,wireframeLineColor.rgb,edgeIntensity*wireframeLineColor.a);
	}
	else if (shaderMenu == 1)
	{
	result = vec4(0,0,0,1.0);

	// Bling-Phong shading
	vec3 lightDir = normalize(worldLightPosition - fragment.position);
	vec3 viewDir = normalize(worldCameraPosition - fragment.position);
	//vec3 normal = normalize(fragment.normal); commented out to use normal map normal
	

	//Ambient light
	vec3 ambient;
	if (hasAmbientTexture) 
	{
		ambient = texture(ambientTexture, fragment.texCoord).rgb;
	} 
	else
	{
		ambient = ambientLightIntensity * ambientColor;
	}

	//Diffuse component
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 diffuse;
	if (hasDiffuseTexture) 
	{
		diffuse = diff * (worldLightIntensity *  texture(diffuseTexture, fragment.texCoord).rgb);
	} else 
	{
		diffuse = diff * (worldLightIntensity * diffuseColor.rgb);
	}
	
	
	//Specular component
	vec3 specular;
	if (hasSpecularTexture) 
	{
		vec3 halfwayDir = normalize(lightDir + viewDir);
		float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
		specular = worldLightIntensity * spec * (texture(specularTexture, fragment.texCoord).rgb * shininessMultiplier);
	}
	else
	{
		if (shininess > 0) { //Compensate for possibly no shininess value
			vec3 halfwayDir = normalize(lightDir + viewDir);
			float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
			specular = worldLightIntensity * spec * (specularColor.rgb * shininessMultiplier);
		}
	}
	
	if (diffuseEnabled) {result.rgb += diffuse;}
	if (specularEnabled) {result.rgb += specular;}
	if (ambientEnabled) 
	{
		if (hasAmbientTexture) {result.rgb *= ambient;}
		else {result.rgb += ambient;}
	
	}

	} 
	else if (shaderMenu == 2)
	{
		// Toon shading code goes here
		vec4 toonResult;

		vec3 lightDir = normalize(worldLightPosition - fragment.position);
		//vec3 normal = normalize(fragment.normal); Commented out to use the new normal mapping normal

		// Calculate lighting intensity 
		float lightIntensity = dot(normal, lightDir);
		//lightIntensity += 0.5;

		// Define thresholds for the different toon shading levels
		float threshold1 = 0.2;  
		float threshold2 = 0.5;
		float threshold3 = 0.7;

		// Quantize the diffuse color based on lightIntensity and thresholds
		vec3 quantizedDiffuseColor;

		//Diffuse component
		float diff = max(dot(normal, lightDir), 0.0);
		vec3 diffuse;
		if (hasDiffuseTexture) 
		{
			diffuse = diff * (worldLightIntensity * texture(diffuseTexture, fragment.texCoord).rgb);
		} else 
		{
			diffuse = diff * (worldLightIntensity * diffuseColor.rgb);
		}

		if (lightIntensity > threshold3) {
			quantizedDiffuseColor = round(diffuse * 12.0) / 12.0;  
		} else if (lightIntensity > threshold2) {
			quantizedDiffuseColor = round(diffuse * 8.0) / 8.0;  
		} else if (lightIntensity > threshold1) {
			quantizedDiffuseColor = round(diffuse * 4.0) / 4.0;  
		} else {
			quantizedDiffuseColor = round(diffuse * 2.0) / 2.0;  
		}

		// Apply the quantized color to the toon shading result
		float lightScale = 2;
		toonResult.rgb = quantizedDiffuseColor * lightScale;

		// Apply toon shading result to the final result
		result = toonResult;
	}

	fragColor = result;
}