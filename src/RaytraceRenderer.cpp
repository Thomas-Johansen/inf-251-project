#include "RaytraceRenderer.h"
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

using namespace minity;
using namespace gl;
using namespace glm;
using namespace globjects;

RaytraceRenderer::RaytraceRenderer(Viewer* viewer) : Renderer(viewer)
{
	m_quadVertices->setStorage(std::array<vec2, 4>({ vec2(-1.0f, 1.0f), vec2(-1.0f,-1.0f), vec2(1.0f,1.0f), vec2(1.0f,-1.0f) }), gl::GL_NONE_BIT);
	auto vertexBindingQuad = m_quadArray->binding(0);
	vertexBindingQuad->setBuffer(m_quadVertices.get(), 0, sizeof(vec2));
	vertexBindingQuad->setFormat(2, GL_FLOAT);
	m_quadArray->enable(0);
	m_quadArray->unbind();

	createShaderProgram("raytrace", {
			{ GL_VERTEX_SHADER,"./res/raytrace/raytrace-vs.glsl" },
			{ GL_FRAGMENT_SHADER,"./res/raytrace/raytrace-fs.glsl" },
		}, 
		{ "./res/raytrace/raytrace-globals.glsl" });
}

void RaytraceRenderer::display()
{
	// Save OpenGL state
	auto currentState = State::currentState();

	// retrieve/compute all necessary matrices and related properties
	const mat4 modelViewProjectionMatrix = viewer()->modelViewProjectionTransform();
	const mat4 inverseModelViewProjectionMatrix = inverse(modelViewProjectionMatrix);

	auto shaderProgramRaytrace = shaderProgram("raytrace");

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	shaderProgramRaytrace->setUniform("modelViewProjectionMatrix", modelViewProjectionMatrix);
	shaderProgramRaytrace->setUniform("inverseModelViewProjectionMatrix", inverseModelViewProjectionMatrix);

	//Raytrace test code
	//Sphere
	glm::vec3 sphereCenter(0.0f, 0.0f, 0.0f);
	float sphereRadius = 1.0f;
	

	shaderProgramRaytrace->setUniform("sphereCenter", sphereCenter);
	shaderProgramRaytrace->setUniform("sphereRadius", sphereRadius);

	//Box
	glm::vec3 boxCenter(1.0f, -5.0f, 1.0f);
	glm::vec3 boxDimensions(2.0f, 2.0f, 2.0f); // Width, Height, Depth
	glm::vec3 boxOrientation(0.0f, 0.0f, 0.0f);

	shaderProgramRaytrace->setUniform("boxCenter", boxCenter);
	shaderProgramRaytrace->setUniform("boxDimensions", boxDimensions);
	shaderProgramRaytrace->setUniform("boxOrientation", boxOrientation);

	//Plane
	glm::vec3 planePoint(0.0f, -10.0f, 0.0f); // A point on the plane
	glm::vec3 planeNormal(0.0f, 1.0f, 0.0f); // Normal vector

	shaderProgramRaytrace->setUniform("planeCenter", planePoint);
	shaderProgramRaytrace->setUniform("planeNormal", planeNormal);

	//Cylinder
	glm::vec3 cylinderBaseCenter(-5.0f, -1.0f, -5.0f);
	glm::vec3 cylinderAxis(0.0f, 1.0f, 0.0f); // This could be the direction
	float cylinderRadius = 0.5f;
	float cylinderHeight = 1.0f;

	shaderProgramRaytrace->setUniform("cylinderBaseCenter", cylinderBaseCenter);
	shaderProgramRaytrace->setUniform("cylinderAxis", cylinderAxis);
	shaderProgramRaytrace->setUniform("cylinderRadius", cylinderRadius);
	shaderProgramRaytrace->setUniform("cylinderHeight", cylinderHeight);



	m_quadArray->bind();
	shaderProgramRaytrace->use();
	// we are rendering a screen filling quad (as a tringle strip), so we can cast rays for every pixel
	m_quadArray->drawArrays(GL_TRIANGLE_STRIP, 0, 4);
	shaderProgramRaytrace->release();
	m_quadArray->unbind();


	// Restore OpenGL state (disabled to to issues with some Intel drivers)
	// currentState->apply();
}