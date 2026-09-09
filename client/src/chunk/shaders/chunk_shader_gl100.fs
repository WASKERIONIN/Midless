/**
 * Copyright (c) 2021-2022 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

"#version 100\n"
"precision mediump float;"
"varying vec2 fragTexCoord;"
"varying vec4 fragColor;"
"varying mediump float fogDistance;"
"varying vec4 sunFragColor;"
"uniform sampler2D texture0;"
"uniform float sunlightStrength;"
"uniform vec3 fogColor;"
"uniform float fogStart;"
"uniform float fogEnd;"
"void main() {"
"   vec4 texelColor = texture2D(texture0, fragTexCoord);"
"   if(texelColor.a == 0.0) discard;"
"   vec3 ambient = vec3(0.11, 0.07, 0.18);"
"   vec4 litColor = texelColor * clamp(sunFragColor * sunlightStrength + fragColor + vec4(ambient, 0.0), vec4(0.16, 0.12, 0.20, 1.0), vec4(1.0));"
"   float fogAmount = smoothstep(fogStart, fogEnd, fogDistance);"
"   vec3 rim = vec3(0.10, 0.03, 0.16) * fogAmount;"
"   vec3 graded = litColor.rgb * vec3(1.02, 0.96, 1.08) + rim;"
"   gl_FragColor = vec4(mix(graded, fogColor, fogAmount), litColor.a);"
"}"
