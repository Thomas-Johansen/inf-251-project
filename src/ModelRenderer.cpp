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


// Function for Catmull-Rom interpolation
float CatmullRomScalarInterpolation(float t, float p0, float p1, float p2, float p3) {
	// Catmull-Rom spline equation for scalars
	float t2 = t * t;
	float t3 = t2 * t;
	return 0.5f * ((2.0f * p1) +
		(-p0 + p2) * t +
		(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
		(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

glm::vec3 CatmullRomVectorInterpolation(float t, const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3) {
	// Catmull-Rom spline equation for vectors
	float t2 = t * t;
	float t3 = t2 * t;
	return 0.5f * ((2.0f * p1) +
		(-p0 + p2) * t +
		(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
		(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

glm::mat4 CatmullRomMatrixInterpolation(float t, const glm::mat4& p0, const glm::mat4& p1, const glm::mat4& p2, const glm::mat4& p3) {
	// Catmull-Rom spline equation for matrix
	float t2 = t * t;
	float t3 = t2 * t;
	return 0.5f * ((2.0f * p1) +
		(-p0 + p2) * t +
		(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
		(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

glm::quat CatmullRomQuaternionInterpolation(float t, const glm::quat& p0, const glm::quat& p1, const glm::quat& p2, const glm::quat& p3) {
	// Catmull-Rom spline equation for quaternions
	float t2 = t * t;
	float t3 = t2 * t;
	return (0.5f * (2.0f * p1) +
		(-p0 + p2) * t +
		(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
		(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}



std::vector<float> CatmullRomInterpolateList(const std::vector<float>& inputValues, int numSegments) { //Catmul rom for list of scalar values, returns numSegments amount of of interpolated values 
	std::vector<float> result;

	if (inputValues.size() < 4) {
		return result;
	}

	float step = 1.0 / numSegments;
	for (int i = 0; i < inputValues.size() - 3; i++) {
		for (int j = 0; j <= numSegments; j++) {
			float t = j * step;
			float interpolatedValue = CatmullRomScalarInterpolation(
				t,
				inputValues[i],
				inputValues[i + 1],
				inputValues[i + 2],
				inputValues[i + 3]
			);
			result.push_back(interpolatedValue);
		}
	}

	return result;
}

glm::quat slerp(float t, const glm::quat& q1, const glm::quat& q2) {
	// Ensure quaternions are unit quaternions
	glm::quat start = glm::normalize(q1);
	glm::quat end = glm::normalize(q2);

	// Calculate the dot product between the quaternions
	float dot = glm::dot(start, end);

	// If the dot product is negative, negate one of the quaternions to take the shortest path
	if (dot < 0.0f) {
		end = -end;
		dot = -dot;
	}

	// Clamp the dot product to ensure it's within the valid range
	dot = glm::clamp(dot, -1.0f, 1.0f);

	// Calculate the angle and axis for interpolation
	float theta = glm::acos(dot);
	float sinTheta = glm::sin(theta);

	// Perform slerp interpolation
	if (sinTheta > 0.001f) {
		float invSinTheta = 1.0f / sinTheta;
		float coeff0 = glm::sin((1.0f - t) * theta) * invSinTheta;
		float coeff1 = glm::sin(t * theta) * invSinTheta;
		return (coeff0 * start) + (coeff1 * end);
	}
	else {
		// If sinTheta is close to zero, use linear interpolation as a fallback
		return (1.0f - t) * start + t * end;
	}
}

mat4 interpolateMatrix(float t, const glm::mat4& p0, const glm::mat4& p1, const glm::mat4& p2, const glm::mat4& p3)
{
	bool preRotation = false;

	glm::vec3 p0_translation;
	glm::quat p0_rotation;
	glm::vec3 p0_scale;
	matrixDecompose2(p0, p0_translation, p0_rotation, p0_scale, preRotation);

	glm::vec3 p1_translation;
	glm::quat p1_rotation;
	glm::vec3 p1_scale;
	matrixDecompose2(p1, p1_translation, p1_rotation, p1_scale, preRotation);

	glm::vec3 p2_translation;
	glm::quat p2_rotation;
	glm::vec3 p2_scale;
	matrixDecompose2(p2, p2_translation, p2_rotation, p2_scale, preRotation);

	glm::vec3 p3_translation;
	glm::quat p3_rotation;
	glm::vec3 p3_scale;
	matrixDecompose2(p3, p3_translation, p3_rotation, p3_scale, preRotation);

	// Interpolate the components
	glm::vec3 interpolatedTranslation = CatmullRomVectorInterpolation(t, p0_translation, p1_translation, p2_translation, p3_translation); 
	glm::quat interpolatedRotation = slerp(t, p1_rotation, p2_rotation);
	glm::vec3 interpolatedScale = CatmullRomVectorInterpolation(t, p0_scale, p1_scale, p2_scale, p3_scale); 

	glm::mat4 result = glm::translate(glm::mat4(1.0f), interpolatedTranslation) * glm::mat4_cast(interpolatedRotation) * glm::scale(glm::mat4(1.0f), interpolatedScale);

	return result;
}

float animationFloat = 0;
bool animationBool = false;
std::vector<float> explodedList;
std::vector<mat4> viewList;
std::vector<mat4> lightList;


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
	

	if (viewer()->doKeyFrame())
	{
		viewer()->didKeyFrame();
		if (explodedList.size() < 4)
		{
			explodedList.push_back(explodedFloat);
			viewList.push_back(viewMatrix);
			lightList.push_back(modelLightMatrix);
			std::cout << "Keyframes: " << explodedList.size() << std::endl;
		}
		
	}
	if (viewer()->doDeleteKeyFrame())
	{
		viewer()->didDeleteKeyFrame();
		if (!explodedList.empty())
		{
			explodedList.pop_back();
			viewList.pop_back();
			lightList.pop_back();
			std::cout << "Keyframes: " << explodedList.size() << std::endl;
		}
	}
	/*
	glm::vec3 colorVector(0, 0, 0);
	glm::vec3 vec1(25.0f, 60.0f, 100.0f);
	glm::vec3 vec2(0.0f, 0.0f, 0.0f);
	glm::vec3 vec3(255.0f, 255.0f, 255.0f);
	glm::vec3 vec4(240.0f, 140.0f, 40.0f);
	colorVector = CatmullRomVectorInterpolation(animationFloat, vec1, vec2, vec3, vec4);
	//std::cout << "Vector x: " << colorVector.x << "  Vector y: " << colorVector.y << "  Vector z: " << colorVector.z << std::endl; //The actuall vector is correct
	viewer()->setBackgroundColor(colorVector);
	*/
	if (viewer()->doAnimation())
	{
		if (explodedList.size() == 4)
		{
			if (animationFloat <= 3)
			{
				mat4 newView;
				mat4 newLight;
				if (animationFloat <= 1)
				{
					explodedFloat = CatmullRomScalarInterpolation(animationFloat, explodedList.at(0), explodedList.at(0), explodedList.at(1), explodedList.at(2));
					newView = interpolateMatrix(animationFloat, viewList.at(0), viewList.at(0), viewList.at(1), viewList.at(2));
					newLight = interpolateMatrix(animationFloat, lightList.at(0), lightList.at(0), lightList.at(1), lightList.at(2));
				}
				else if (animationFloat <= 2)
				{
					explodedFloat = CatmullRomScalarInterpolation((animationFloat - 1), explodedList.at(0), explodedList.at(1), explodedList.at(2), explodedList.at(3));
					newView = interpolateMatrix((animationFloat -1), viewList.at(0), viewList.at(1), viewList.at(2), viewList.at(3));
					newLight = interpolateMatrix((animationFloat - 1), lightList.at(0), lightList.at(1), lightList.at(2), lightList.at(3));
				}
				else if (animationFloat <= 3)
				{
					explodedFloat = CatmullRomScalarInterpolation((animationFloat - 2), explodedList.at(1), explodedList.at(2), explodedList.at(3), explodedList.at(3));
					newView = interpolateMatrix((animationFloat - 2), viewList.at(1), viewList.at(2), viewList.at(3), viewList.at(3));
					newLight = interpolateMatrix((animationFloat - 2), lightList.at(1), lightList.at(2), lightList.at(3), lightList.at(3));
				}

				std::vector<Vertex> vertices = viewer()->scene()->model()->vertices(); 
				for (uint i = 0; i < groups.size(); i++) 
				{
					auto& group = groups.at(i); 
					auto vector = groupVectors.at(i); 

					for (auto j : group.indexes) 
					{
						Vertex& vertex = vertices.at(j); 
						vertex.position += (explodedFloat * vector); 
					}

				}
				viewer()->setLightTransform(newLight);
				viewer()->setViewTransform(newView);
				viewer()->scene()->model()->vertexBuffer().setSubData(vertices);
				animationFloat += 0.005;
			}
		} else 
		{ std::cout << "Create More Keyframes" << std::endl; viewer()->animationDone(); }
	}
	else { animationFloat = 0; viewer()->animationDone();}
	

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
		if (ImGui::SliderFloat("Explode", &explodedFloat, 0, 5)) 
		{

			std::vector<Vertex> vertices = viewer()->scene()->model()->vertices();
			for (uint i = 0; i < groups.size(); i++)
			{ 
				auto& group = groups.at(i);
				auto vector = groupVectors.at(i);

				for (auto j : group.indexes)
				{
					Vertex& vertex = vertices.at(j);
					vertex.position += ((explodedFloat)*vector);
				}
				
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