#include <et>

layout (std140, set = 0, binding = PassVariablesBufferIndex) uniform PassVariables passVariables;

layout (std140, set = 0, binding = ObjectVariablesBufferIndex) uniform ObjectVariables {
	mat4 worldTransform;
} objectVariables;

layout(binding = 0, set = 1) uniform sampler2D albedoTexture;

struct VSOutput {
	vec2 texCoord0;
};

#include <inputdefines>
#include <stagedefine>

#if defined(ET_VERTEX_SHADER)

#include <inputlayout>

layout(location = 0) out VSOutput vsOut;

void main()
{
	vsOut.texCoord0 = vec2(texCoord0.x, texCoord0.y);

#if (TRANSFORM_INPUT_POSITION)
	gl_Position = passVariables.viewProjection * objectVariables.worldTransform * vec4(position, 1.0);
#else
	gl_Position = vec4(position, 1.0);
#endif
}

#elif defined(ET_FRAGMENT_SHADER)

layout(location = 0) in VSOutput fsIn;
layout(location = 0) out vec4 outColor0;

void main()
{
	outColor0 = texture(albedoTexture, fsIn.texCoord0);
}

#else
#
#error Invalid or unsupported shader
#
#endif