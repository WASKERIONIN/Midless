/**
 * Copyright (c) 2021-2022 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

"#version 330\n"
"in vec2 fragTexCoord;"
"in vec4 fragColor;"
"in float fogDistance;"
"in vec4 sunFragColor;"
"uniform sampler2D texture0;"
"uniform float sunlightStrength;"
"uniform vec3 fogColor;"
"uniform float fogStart;"
"uniform float fogEnd;"
"out vec4 finalColor;"
"void main() {"
"   vec4 texelColor = texture(texture0, fragTexCoord);"
"   if(texelColor.a == 0.0) discard;"
"   vec3 ambient = vec3(0.13, 0.10, 0.26);"
"   vec4 litColor = texelColor * clamp(sunFragColor * sunlightStrength + fragColor + vec4(ambient, 0.0), vec4(0.18, 0.12, 0.22, 1.0), vec4(1.0));"
"   float fogAmount = smoothstep(fogStart, fogEnd, fogDistance);"
"   vec3 rim = vec3(0.16, 0.05, 0.30) * fogAmount;"
"   vec3 graded = litColor.rgb * vec3(1.05, 0.98, 1.18) + rim;"
"   finalColor = vec4(mix(graded, fogColor, fogAmount), litColor.a);"
"}"
