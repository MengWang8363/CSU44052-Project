#version 330 core

// Input
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexColor;
layout(location = 2) in vec3 vertexNormal;
layout(location = 3) in vec2 vertexUV;

// Output data, to be interpolated for each fragment
out vec3 color;

out vec3 worldPosition;
out vec3 worldNormal;
out vec4 fragPosLightSpace;

out vec2 uv;

uniform mat4 MVP;
uniform mat4 lightSpaceMatrix;

void main() {
    // Transform vertex
    gl_Position =  MVP * vec4(vertexPosition, 1);
    
    // Pass vertex color to the fragment shader
    color = vertexColor;

    uv = vertexUV;   

    // World-space geometry 
    worldPosition = vertexPosition;
    worldNormal = vertexNormal;

    // Transform position into light space
    fragPosLightSpace = lightSpaceMatrix * vec4(worldPosition, 1.0);

}
