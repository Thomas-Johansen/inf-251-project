#version 400
#extension GL_ARB_shading_language_include : require
#include "/raytrace-globals.glsl"

uniform mat4 modelViewProjectionMatrix;
uniform mat4 inverseModelViewProjectionMatrix;

in vec2 fragPosition;
out vec4 fragColor;

//Assignment 4 imports
uniform vec3 sphereCenter;
uniform float sphereRadius;

uniform vec3 boxCenter;
uniform vec3 boxDimensions;  // Width, height, and depth
uniform vec3 boxOrientation; 

uniform vec3 planeCenter;
uniform vec3 planeNormal; 

uniform vec3 cylinderBaseCenter;
uniform vec3 cylinderAxis; // This should be normalized
uniform float cylinderHeight;
uniform float cylinderRadius;

float calcDepth(vec3 pos)
{
	float far = gl_DepthRange.far; 
	float near = gl_DepthRange.near;
	vec4 clip_space_pos = modelViewProjectionMatrix * vec4(pos, 1.0);
	float ndc_depth = clip_space_pos.z / clip_space_pos.w;
	return (((far - near) * ndc_depth) + near + far) / 2.0;
}


vec3 sphereColor(vec3 hitPoint) {
    vec3 normal = normalize(hitPoint - sphereCenter);
    return 0.5 * (normal + 1.0); // Map [-1, 1] to [0, 1]
}

vec3 cubeColor(vec3 hitPoint, vec3 boxMin, vec3 boxMax) {
    // Determine the side of the cube that was hit
    vec3 boxCenter = (boxMin + boxMax) * 0.5;
    vec3 localPoint = hitPoint - boxCenter;  // Transform hit point to box's local space

    vec3 color;

    if (abs(localPoint.x) > abs(localPoint.y) && abs(localPoint.x) > abs(localPoint.z)) {
        // Hit on X face
        color = (localPoint.x > 0.0) ? vec3(1.0, 0.0, 0.0) : vec3(0.5, 0.0, 0.0); // Red shades
    } else if (abs(localPoint.y) > abs(localPoint.z)) {
        // Hit on Y face
        color = (localPoint.y > 0.0) ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.5, 0.0); // Green shades
    } else {
        // Hit on Z face
        color = (localPoint.z > 0.0) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 0.0, 0.5); // Blue shades
    }

    return color;
}

vec3 planeColor(vec3 hitPoint) {
    // Example: simple checkerboard pattern
    float patternScale = 0.1; // Scale of the checker pattern
    float pattern = mod(floor(hitPoint.x * patternScale) + floor(hitPoint.z * patternScale), 2.0);
    vec3 color = mix(vec3(1.0), vec3(0.0), pattern); // White and black pattern
    return color;
}

// Sphere intersection function
bool intersectSphere(vec3 rayOrigin, vec3 rayDirection, vec3 center, float radius, out float t) {
    vec3 oc = rayOrigin - center;
    float a = dot(rayDirection, rayDirection);
    float b = 2.0 * dot(oc, rayDirection);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;

    if (discriminant > 0.0) {
        float t1 = (-b - sqrt(discriminant)) / (2.0 * a);
        float t2 = (-b + sqrt(discriminant)) / (2.0 * a);

        t = (t1 < t2 && t1 > 0.0) ? t1 : t2;
        return t > 0.0;
    }
    return false;
}

// Box intersection function
bool intersectBox(vec3 rayOrigin, vec3 rayDirection, vec3 boxMin, vec3 boxMax, out float tMin, out float tMax) {
    vec3 invDir = 1.0 / rayDirection;
    vec3 t0s = (boxMin - rayOrigin) * invDir;
    vec3 t1s = (boxMax - rayOrigin) * invDir;

    vec3 tMinVec = min(t0s, t1s);
    vec3 tMaxVec = max(t0s, t1s);

    tMin = max(max(tMinVec.x, tMinVec.y), tMinVec.z);
    tMax = min(min(tMaxVec.x, tMaxVec.y), tMaxVec.z);

    return tMax >= max(0.0, tMin);
}

bool intersectPlane(vec3 rayOrigin, vec3 rayDirection, vec3 planeCenter, vec3 planeNormal, out float t) {
    // Calculate the denominator to ensure it's not zero (plane and ray are not parallel)
    float denom = dot(rayDirection, planeNormal);
    if (abs(denom) > 0.0001) { // Use an epsilon to avoid division by zero
        vec3 p0l0 = planeCenter - rayOrigin;
        t = dot(p0l0, planeNormal) / denom;
        return (t >= 0.0); // Intersection is in the ray direction
    }
    return false; // Ray is parallel to the plane
}

bool intersectCylinder(vec3 rayOrigin, vec3 rayDirection, vec3 baseCenter, vec3 axis, float height, float radius, out float t) {
    // Translate the ray origin to the coordinate system of the cylinder
    vec3 dp = rayOrigin - baseCenter;
    float a = dot(rayDirection, rayDirection) - dot(rayDirection, axis) * dot(rayDirection, axis);
    float b = 2.0 * (dot(rayDirection, dp) - (dot(rayDirection, axis) * dot(dp, axis)));
    float c = dot(dp, dp) - dot(dp, axis) * dot(dp, axis) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;

    if (discriminant < 0.0) return false; // No intersection

    float sqrtDiscriminant = sqrt(discriminant);
    float t0 = (-b - sqrtDiscriminant) / (2.0 * a);
    float t1 = (-b + sqrtDiscriminant) / (2.0 * a);

    // Ensure t0 is the smaller t value
    if (t0 > t1) {
        float temp = t0;
        t0 = t1;
        t1 = temp;
    }

    float y0 = dot(rayOrigin + t0 * rayDirection - baseCenter, axis);
    float y1 = dot(rayOrigin + t1 * rayDirection - baseCenter, axis);

    // Check if the intersection points are within the height of the cylinder
    if (y0 < 0.0) {
        if (y1 < 0.0) return false; // Both points are below the base
        // The ray hits the side of the cylinder
        t0 = t0 + (t1 - t0) * (y0 / (y0 - y1));
    } else if (y0 >= height) {
        if (y1 >= height) return false; // Both points are above the top
        // The ray hits the top cap of the cylinder
        t0 = t0 + (t1 - t0) * ((y0 - height) / (y0 - y1));
    }

    // If t0 is negative, the cylinder is behind the ray
    if (t0 < 0.0) {
        if (y1 < 0.0 || y1 >= height) return false; // The ray origin is inside the cylinder, but we're only considering forward intersections
        t0 = t1;
    }

    t = t0;
    return true;
}


void main()
{
	vec4 near = inverseModelViewProjectionMatrix*vec4(fragPosition,-1.0,1.0);
	near /= near.w;

	vec4 far = inverseModelViewProjectionMatrix*vec4(fragPosition,1.0,1.0);
	far /= far.w;

	// this is the setup for our viewing ray
	vec3 rayOrigin = near.xyz;
	vec3 rayDirection = normalize((far-near).xyz);

    float tSphere, tBoxMin, tBoxMax, tPlane;
    bool hitSphere = intersectSphere(rayOrigin, rayDirection, sphereCenter, sphereRadius, tSphere);
    bool hitBox = intersectBox(rayOrigin, rayDirection, boxCenter - boxDimensions * 0.5, boxCenter + boxDimensions * 0.5, tBoxMin, tBoxMax);
    bool hitPlane = intersectPlane(rayOrigin, rayDirection, planeCenter, planeNormal, tPlane);
    float tCylinder;
    bool hitCylinder = intersectCylinder(rayOrigin, rayDirection, cylinderBaseCenter, cylinderAxis, cylinderHeight, cylinderRadius, tCylinder);

    float INFINITY = 1000;

    float closestT = INFINITY;
    vec3 color = vec3(0.0); // Background color

    // Check if the sphere was hit and is the closest
    if (hitSphere) {
        closestT = tSphere;
        vec3 hitPoint = rayOrigin + tSphere * rayDirection;
        color = sphereColor(hitPoint);
    }

    // Check if the box was hit and is closer than the current closest
    if (hitBox && tBoxMin < closestT) {
        closestT = tBoxMin;
        vec3 hitPoint = rayOrigin + tBoxMin * rayDirection;
        vec3 boxMin = boxCenter - boxDimensions * 0.5;
        vec3 boxMax = boxCenter + boxDimensions * 0.5;
        color = cubeColor(hitPoint, boxMin, boxMax);
    }

    // Check if the plane was hit and is closer than the current closest
    if (hitPlane && tPlane < closestT) {
        closestT = tPlane;
        vec3 hitPoint = rayOrigin + tPlane * rayDirection;
        color = planeColor(hitPoint);
    }

    if (hitCylinder && tCylinder < closestT) {
        closestT = tCylinder;
        // Calculate cylinder color or shading here
        // Placeholder color for the cylinder
        color = vec3(0.5, 0.3, 0.2); 
    }

    // Set the fragment color and depth
    fragColor = vec4(color, 1.0);

    if (closestT < INFINITY) {
        gl_FragDepth = calcDepth(rayOrigin + closestT * rayDirection);
    } else {
        // No intersection, set depth to the farthest point
        gl_FragDepth = calcDepth(rayOrigin + rayDirection * gl_DepthRange.far);
    }
}