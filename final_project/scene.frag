#version 330 core

in vec3 color;
in vec3 worldPosition;
in vec3 worldNormal;
in vec4 fragPosLightSpace;

in vec2 uv; 

uniform vec3 lightPosition;
uniform vec3 lightIntensity;
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;

uniform sampler2D textureSampler;

out vec4 finalColor;

// Shadow bias to prevent acne
const float bias = 0.005;

// Function to calculate shadow factor
float calculateShadow(vec4 fragPosLightSpace) {
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform to [0, 1] range
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0) return 1.0;

    // Retrieve depth from shadow map
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    float currentDepth = projCoords.z;

    // Compare depths and return shadow factor
	float shadow = (currentDepth >= closestDepth + bias) ? 0.2 : 1.0;
    return shadow;
}

void main()
{
	// TODO: lighting, tone mapping, gamma correction
	vec3 color_temp = color;
	// lighting
	vec3 diffuseReflectance = color / 3.14159;

	vec3 N = normalize(worldNormal);
    vec3 L = normalize(lightPosition - worldPosition);
	float cosine = max(dot(N, L), 0.0);

	float distance = length(lightPosition - worldPosition);
	vec3 irradiance = lightIntensity / ( 0.6 * 3.14159 * distance * distance);

	vec3 color = diffuseReflectance * cosine * irradiance;

	// Calculate shadow factor
    float shadow = calculateShadow(fragPosLightSpace);

	color = color * shadow;

	// tone mapping
	color = color / (1.0 + color);

	// gamma correction
	color = pow(color, vec3(1.0 / 2.2));

	vec3 textureColor = texture(textureSampler, uv).rgb;

	finalColor = vec4(textureColor * color, 1.0);
}
