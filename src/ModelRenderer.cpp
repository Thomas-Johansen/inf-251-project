#include "ModelRenderer.h"
#include <globjects/base/File.h>
#include <globjects/State.h>
#include <iostream>
#include <filesystem>
#include <imgui.h>
#include "Viewer.h"
#include "Scene.h"
#include "Model.h"
#include <sstream>


#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <stb_image.h>

using namespace minity;
using namespace gl;
using namespace glm;
using namespace globjects;

ModelRenderer::ModelRenderer(Viewer* viewer) : Renderer(viewer)
{
	m_lightVertices->setStorage(std::array<vec3, 1>({ vec3(0.0f) }), GL_NONE_BIT);
	auto lightVertexBinding = m_lightArray->binding(0);
	lightVertexBinding->setBuffer(m_lightVertices.get(), 0, sizeof(vec3));
	lightVertexBinding->setFormat(3, GL_FLOAT);
	m_lightArray->enable(0);
	m_lightArray->unbind();

	createShaderProgram("model-base", {
		{ GL_VERTEX_SHADER,"./res/model/model-base-vs.glsl" },
		{ GL_GEOMETRY_SHADER,"./res/model/model-base-gs.glsl" },
		{ GL_FRAGMENT_SHADER,"./res/model/model-base-fs.glsl" },
		}, 
		{ "./res/model/model-globals.glsl" });

	createShaderProgram("model-light", {
		{ GL_VERTEX_SHADER,"./res/model/model-light-vs.glsl" },
		{ GL_FRAGMENT_SHADER,"./res/model/model-light-fs.glsl" },
		}, { "./res/model/model-globals.glsl" });
}


//Skybox test
/*
std::vector<std::string> faces
{
	"C:/Users/thoma/Downloads/project/res/skybox/right.jpg",
	"C:/Users/thoma/Downloads/project/res/skybox/left.jpg",
	"C:/Users/thoma/Downloads/project/res/skybox/top.jpg",
	"C:/Users/thoma/Downloads/project/res/skybox/bottom.jpg",
	"C:/Users/thoma/Downloads/project/res/skybox/front.jpg",
	"C:/Users/thoma/Downloads/project/res/skybox/back.jpg"
};
//unsigned int cubemapTexture = loadCubemap(faces);

unsigned int loadCubemap(std::vector<std::string> faces)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height, nrChannels;
	for (unsigned int i = 0; i < faces.size(); i++)
	{
		unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
		if (data)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
				0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
			);
			stbi_image_free(data);
		}
		else
		{
			std::cout << "Cubemap tex failed to load at path: " << faces[i] << std::endl;
			stbi_image_free(data);
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
}

*/



void ModelRenderer::display()
{
	// Save OpenGL state
	auto currentState = State::currentState();

	// retrieve/compute all necessary matrices and related properties
	const mat4 viewMatrix = viewer()->viewTransform();
	const mat4 inverseViewMatrix = inverse(viewMatrix);
	const mat4 modelViewMatrix = viewer()->modelViewTransform();
	const mat4 inverseModelViewMatrix = inverse(modelViewMatrix);
	const mat4 modelLightMatrix = viewer()->modelLightTransform();
	const mat4 inverseModelLightMatrix = inverse(modelLightMatrix);
	const mat4 modelViewProjectionMatrix = viewer()->modelViewProjectionTransform();
	const mat4 inverseModelViewProjectionMatrix = inverse(modelViewProjectionMatrix);
	const mat4 projectionMatrix = viewer()->projectionTransform();
	const mat4 inverseProjectionMatrix = inverse(projectionMatrix);
	const mat3 normalMatrix = mat3(transpose(inverseModelViewMatrix));
	const mat3 inverseNormalMatrix = inverse(normalMatrix);
	const vec2 viewportSize = viewer()->viewportSize();

	auto shaderProgramModelBase = shaderProgram("model-base");

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	viewer()->scene()->model()->vertexArray().bind();

	const std::vector<Group> & groups = viewer()->scene()->model()->groups();
	const std::vector<Material> & materials = viewer()->scene()->model()->materials();

	static std::vector<bool> groupEnabled(groups.size(), true);
	static bool wireframeEnabled = true;
	static bool lightSourceEnabled = true;
	static vec4 wireframeLineColor = vec4(1.0f);

	//New menu options for Shader
	static bool blinnPhongEnabled = false;
	static bool ambientEnabled = true;
	static bool diffuseEnabled = true;
	static bool specularEnabled = true;
	static bool toonEnabled = false;
	static int shaderMenu = 1;
	//Light options
	static vec3 worldLightIntensity = vec3(1,1,1);
	static vec3 ambientLightIntensity = vec3(0.1f,0.08f,0.06f);
	static float shininessMultiplier = 1.0f;

	//Normal mapping
	static int normalMenu = 0;
	//Bump mapping
	static int bumpMenu = 0;
	static float bumpAmplitude = 0.5f;
	static float bumpWavenumber = 5;

	//Assignment 3
	//Animation
	static float explodedFloat = 0;
	const std::vector<vec3>& groupVectors = viewer()->scene()->model()->groupVectors();

	

	if (ImGui::BeginMenu("Model"))
	{
		//Shader
		ImGui::RadioButton("Wireframe Enabled", &shaderMenu, 0);
		ImGui::RadioButton("Blinn-Phong Enabled", &shaderMenu, 1);
		ImGui::RadioButton("Toon Enabled", &shaderMenu, 2);
		//ImGui::RadioButton("Skybox Enabled", &shaderMenu, 3);
		//Normal mapping
		ImGui::Separator();
		ImGui::RadioButton("No normal map", &normalMenu, 0);
		ImGui::RadioButton("Object Space Normal", &normalMenu, 1);
		ImGui::RadioButton("Tangent Space Normal", &normalMenu, 2);
		//Bump mapping
		ImGui::Separator();
		ImGui::RadioButton("No bumb mapping", &bumpMenu, 0);
		ImGui::RadioButton("Bump funciton 1", &bumpMenu, 1);
		ImGui::RadioButton("Bump funciton 2", &bumpMenu, 2);
		//Animation
		ImGui::Separator();
		if (ImGui::SliderFloat("Explode", &explodedFloat, 0, 2)) 
		{
			std::vector<Vertex> vertices = viewer()->scene()->model()->vertices();
			//for (uint i = 0; i < groups.size(); i++)
			
			auto &group = groups.at(0);
			auto vector = groupVectors.at(0);
			//std::cout << "Group Indexes: " << group.indexes.size() << std::endl;
			
			for (auto j : group.indexes)
			{
				//std::cout << "Index: " << j << std::endl;
				Vertex &vertex = vertices.at(j);
				vertex.position += ((explodedFloat) * vector); 
			}
			viewer()->scene()->model()->vertexBuffer().setSubData(vertices);
		}



		ImGui::Separator();
		ImGui::Checkbox("Light Source Enabled", &lightSourceEnabled);

		
		

		if (shaderMenu == 0)
		{
			if (ImGui::CollapsingHeader("Wireframe"))
			{
				ImGui::ColorEdit4("Line Color", (float*)&wireframeLineColor, ImGuiColorEditFlags_AlphaBar);
			}
		}
		//Blinn Phong options
		if (shaderMenu == 1)
		{
			if (ImGui::CollapsingHeader("Blinn-Phong"))
			{
				ImGui::Checkbox("Ambient Enabled", &ambientEnabled);
				ImGui::Checkbox("Diffuse Enabled", &diffuseEnabled);
				ImGui::Checkbox("Specular Enabled", &specularEnabled);
				ImGui::ColorEdit3("World Light Intensity", (float*)&worldLightIntensity, ImGuiColorEditFlags_AlphaBar);
				ImGui::ColorEdit3("Ambient Light Intensity", (float*)&ambientLightIntensity, ImGuiColorEditFlags_AlphaBar);
				ImGui::SliderFloat("Shininess Multiplier", &shininessMultiplier, 0.0f, 100.0f);

			}
		}
		//Toon options
		//Shader menu == 2

		//Bump options
		if (bumpMenu != 0)
		{
			if (ImGui::CollapsingHeader("Bump Map"))
			{
				ImGui::SliderFloat("Amplitude", &bumpAmplitude, 0.0f, 10.0f);
				ImGui::SliderFloat("Wavenumber", &bumpWavenumber, 0.0f, 10.0f);
			}
		}



		if (ImGui::CollapsingHeader("Groups"))
		{
			for (uint i = 0; i < groups.size(); i++)
			{
				bool checked = groupEnabled.at(i);
				ImGui::Checkbox(groups.at(i).name.c_str(), &checked);
				groupEnabled[i] = checked;
			}

		}





		ImGui::EndMenu();
	}

	vec4 worldCameraPosition = inverseModelViewMatrix * vec4(0.0f, 0.0f, 0.0f, 1.0f);
	vec4 worldLightPosition = inverseModelLightMatrix * vec4(0.0f, 0.0f, 0.0f, 1.0f);

	shaderProgramModelBase->setUniform("modelViewProjectionMatrix", modelViewProjectionMatrix);
	shaderProgramModelBase->setUniform("viewportSize", viewportSize);
	shaderProgramModelBase->setUniform("worldCameraPosition", vec3(worldCameraPosition));
	shaderProgramModelBase->setUniform("worldLightPosition", vec3(worldLightPosition));
	shaderProgramModelBase->setUniform("wireframeEnabled", wireframeEnabled);
	shaderProgramModelBase->setUniform("wireframeLineColor", wireframeLineColor);

	//Light intensity
	shaderProgramModelBase->setUniform("worldLightIntensity", worldLightIntensity);
	shaderProgramModelBase->setUniform("ambientLightIntensity", ambientLightIntensity);


	//Shader Menu options
	shaderProgramModelBase->setUniform("shaderMenu", shaderMenu);
	shaderProgramModelBase->setUniform("ambientEnabled", ambientEnabled);
	shaderProgramModelBase->setUniform("diffuseEnabled", diffuseEnabled);
	shaderProgramModelBase->setUniform("specularEnabled", specularEnabled);
	shaderProgramModelBase->setUniform("shininessMultiplier", shininessMultiplier);

	//Normal mapping
	shaderProgramModelBase->setUniform("normalMenu", normalMenu);
	//Bump mapping
	shaderProgramModelBase->setUniform("bumpMenu", bumpMenu);
	shaderProgramModelBase->setUniform("bumpAmplitude", bumpAmplitude);
	shaderProgramModelBase->setUniform("bumpWavenumber", bumpWavenumber);
		


	
	shaderProgramModelBase->use();



	for (uint i = 0; i < groups.size(); i++)
	{
		if (groupEnabled.at(i))
		{
			const Material & material = materials.at(groups.at(i).materialIndex);

			//Parse ambient specular diffuse color etc:
			shaderProgramModelBase->setUniform("ambientColor", material.ambient);
			shaderProgramModelBase->setUniform("diffuseColor", material.diffuse);
			shaderProgramModelBase->setUniform("specularColor", material.specular);
			shaderProgramModelBase->setUniform("shininess", material.shininess);

			

			if (material.diffuseTexture)
			{
				shaderProgramModelBase->setUniform("diffuseTexture", 0);
				material.diffuseTexture->bindActive(0);
				shaderProgramModelBase->setUniform("hasDiffuseTexture", true);
			}
			else 
			{
				shaderProgramModelBase->setUniform("hasDiffuseTexture", false);
			}
			
			//Specular & Ambient texture
			if (material.ambientTexture)
			{
				shaderProgramModelBase->setUniform("ambientTexture", 1);
				material.ambientTexture->bindActive(1);
				shaderProgramModelBase->setUniform("hasAmbientTexture", true);
			}
			else
			{
				shaderProgramModelBase->setUniform("hasAmbientTexture", false);
			}

			if (material.specularTexture)
			{
				shaderProgramModelBase->setUniform("specularTexture", 2);
				material.specularTexture->bindActive(2);
				shaderProgramModelBase->setUniform("hasSpecularTexture", true);
			}
			else 
			{
				shaderProgramModelBase->setUniform("hasSpecularTexture", false);
			}

			//Normal mapping textures
			//Normal matrix
			shaderProgramModelBase->setUniform("normalMatrix", normalMatrix);
			//Object Space Normal Map
			if (material.objectNormals)
			{
				shaderProgramModelBase->setUniform("objectNormals", 3);
				material.objectNormals->bindActive(3);
				shaderProgramModelBase->setUniform("hasObjectNormals", true);
			}
			else
			{
				shaderProgramModelBase->setUniform("hasObjectNormals", false);
			}
			//Tangent Space Normal Map
			if (material.tangentNormals)
			{
				shaderProgramModelBase->setUniform("tangentNormals", 4);
				material.tangentNormals->bindActive(4);
				shaderProgramModelBase->setUniform("hasTangentNormals", true);
			}
			else
			{
				shaderProgramModelBase->setUniform("hasTangentNormals", false);
			}
			


			viewer()->scene()->model()->vertexArray().drawElements(GL_TRIANGLES, groups.at(i).count(), GL_UNSIGNED_INT, (void*)(sizeof(GLuint)*groups.at(i).startIndex));

			if (material.diffuseTexture)
			{
				material.diffuseTexture->unbind();
			}
			//Specular & ambient
			if (material.ambientTexture)
			{
				material.ambientTexture->unbind();
			}
			if (material.specularTexture)
			{
				material.specularTexture->unbind();
			}
			if (material.objectNormals)
			{
				material.objectNormals->unbind();
			}
			if (material.tangentNormals)
			{
				material.tangentNormals->unbind();
			}
		}
	}

	shaderProgramModelBase->release();

	viewer()->scene()->model()->vertexArray().unbind();


	if (lightSourceEnabled)
	{
		auto shaderProgramModelLight = shaderProgram("model-light");
		shaderProgramModelLight->setUniform("modelViewProjectionMatrix", modelViewProjectionMatrix * inverseModelLightMatrix);
		shaderProgramModelLight->setUniform("viewportSize", viewportSize);

		glEnable(GL_PROGRAM_POINT_SIZE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);

		m_lightArray->bind();

		shaderProgramModelLight->use();
		m_lightArray->drawArrays(GL_POINTS, 0, 1);
		shaderProgramModelLight->release();

		m_lightArray->unbind();

		glDisable(GL_PROGRAM_POINT_SIZE);
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
	}

	// Restore OpenGL state (disabled to to issues with some Intel drivers)
	// currentState->apply();
}