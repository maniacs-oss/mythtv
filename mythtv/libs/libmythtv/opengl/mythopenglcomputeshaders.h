#ifndef MYTHOPENGLCOMPUTESHADERS_H
#define MYTHOPENGLCOMPUTESHADERS_H

#include <QString>

// This is a work in progress
// - assumes either NV12 or YV12
// - will probably break for rectangular and OES textures?
// - assumes HLG to Rec.709
// - various other hardcoded assumptions/settings

static const QString GLSL430Tonemap =
"#extension GL_ARB_compute_shader : enable\n"
"#extension GL_ARB_shader_storage_buffer_object : enable\n"
"#extension GL_ARB_shader_image_load_store : enable\n"
"layout(std430, binding=0) buffer FrameGlobals {\n"
"  highp vec2 m_running;\n"
"  uint m_frameMean;\n"
"  uint m_frameMax;\n"
"  uint m_wgCounter;\n"
"};\n"
"layout(rgba16f) uniform highp writeonly image2D m_texture;\n"
"#ifdef UNSIGNED\n"
"#define sampler2D usampler2D\n"
"#endif\n"
"uniform highp sampler2D texture0;\n"
"uniform highp sampler2D texture1;\n"
"#ifdef YV12\n"
"uniform highp sampler2D texture2;\n"
"#endif\n"
"uniform highp mat4 m_colourMatrix;\n"
"uniform highp mat4 m_primaryMatrix;\n"
"layout(local_size_x = 8, local_size_y = 8) in;\n"
"shared uint m_workGroupMean;\n"
"shared uint m_workGroupMax;\n"
"vec3 hable(vec3 H) {\n"
"  return (H * (0.150000 * H + vec3(0.050000)) + vec3(0.004000)) / \n"
"         (H * (0.150000 * H + vec3(0.500000)) + vec3(0.060000)) - vec3(0.066667);\n"
"}\n"
"void main() {\n"
"  uint numWorkGroups = gl_NumWorkGroups.x * gl_NumWorkGroups.y;\n"
"  highp vec2 coord = (vec2(gl_GlobalInvocationID) + vec2(0.5, 0.5)) / vec2(gl_NumWorkGroups * gl_WorkGroupSize);\n"
"  highp vec4 pixel = vec4(texture(texture0, coord).r,\n"
"#ifdef YV12\n"
"                          texture(texture1, coord).r,\n"
"                          texture(texture2, coord).r,\n"
"#else\n"
"                          texture(texture1, coord).rg,\n"
"#endif\n"
"                          1.0);\n"
"#ifdef UNSIGNED\n"
"  pixel /= vec4(65535.0, 65535.0, 65535.0, 1.0);\n"
"#endif\n"
"  pixel *= m_colourMatrix;\n"
"  pixel  = clamp(pixel, 0.0, 1.0);\n"
"  pixel.rgb = mix(vec3(4.0) * pixel.rgb * pixel.rgb,\n"
"                  exp((pixel.rgb - vec3(0.559911)) * vec3(1.0/0.178833)) +\n"
"                  vec3(0.284669), lessThan(vec3(0.5), pixel.rgb));\n"
"  pixel.rgb *= vec3(0.506970 * pow(dot(vec3(0.2627, 0.6780, 0.0593), pixel.rgb), 0.200000));\n"

"  int channel = 0;\n"
"  if (pixel.g > pixel.r) channel = 1;\n"
"  if (pixel.b > pixel[channel]) channel = 2;\n"
"  float channelmax = pixel[channel];\n"
"  highp float mean = 0.25;\n"
"  highp float peak = 10.0;\n"
"  if (m_running.y > 0.0) {\n"
"    mean = max(0.001, m_running.x);\n"
"    peak = max(1.000, m_running.y);\n"
"  }\n"

"  m_workGroupMean = 0;\n"
"  m_workGroupMax = 0;\n"
"  barrier();\n"
"  atomicAdd(m_workGroupMean, uint(log(max(channelmax, 0.001)) * 400.0));\n"
"  atomicMax(m_workGroupMax,  uint(channelmax * 10000.0));\n"
"  memoryBarrierShared();\n"
"  barrier();\n"

"  if (gl_LocalInvocationIndex == 0) {\n"
"    atomicAdd(m_frameMean, uint(m_workGroupMean / uint(gl_WorkGroupSize.x * gl_WorkGroupSize.y)));\n"
"    atomicMax(m_frameMax,  m_workGroupMax);\n"
"    memoryBarrierBuffer();\n"
"  }\n"
"  barrier();\n"

"  if (gl_LocalInvocationIndex == 0 && atomicAdd(m_wgCounter, 1) == (numWorkGroups - 1)) {\n"
"    highp vec2 current = vec2(exp(float(m_frameMean) / (float(numWorkGroups) * 400.0)),\n"
"                              float(m_frameMax) / 10000.0);\n"
"    if (m_running.y == 0.0) m_running = current;\n"
"    m_running += 0.05 * (current - m_running);\n"
"    float weight = smoothstep(1.266422, 2.302585, abs(log(current.x / m_running.x)));\n"
"    m_running = mix(m_running, current, weight);\n"
"    m_wgCounter = 0;\n"
"    m_frameMean = 0;\n"
"    m_frameMax = 0;\n"
"    memoryBarrierBuffer();\n"
"  }\n"

"  vec3 colour = pixel.rgb;\n"
"  float slope = min(1.000000, 0.25 / mean);\n"
"  colour *= slope;\n"
"  peak   *= slope;\n"
"  colour  = hable(max(vec3(0.0), colour)) / hable(vec3(peak)).x;\n"
"  colour  = min(colour, vec3(1.0));\n"
"  vec3 linear = pixel.rgb * (colour[channel] / channelmax);\n"
"  float coeff = max(colour[channel] - 0.180000, 1e-6) / max(colour[channel], 1.0);\n"
"  coeff = 0.750000 * pow(coeff, 1.500000);\n"
"  pixel.rgb = mix(linear, colour, coeff);\n"

// BT2020 to Rec709
"  pixel = m_primaryMatrix * clamp(pixel, 0.0, 1.0);\n"
"  imageStore(m_texture, ivec2(gl_GlobalInvocationID), vec4(pow(pixel.rgb, vec3(1.0 / 2.2)), 1.0));\n"
"}\n";

#endif // MYTHOPENGLCOMPUTESHADERS_H
