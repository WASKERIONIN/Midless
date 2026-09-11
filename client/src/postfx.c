/*
 * Midless: Cosmic Edition — full-screen post processing.
 *
 * Chain: gravity lensing around the black hole -> radial chromatic
 * aberration -> bloom -> nebula colour grade (indigo shadows, teal mids,
 * warm highlights) -> filmic tonemap -> vignette -> film grain.
 */

#include "postfx.h"
#include "raylib.h"
#include "rlgl.h"
#include <math.h>

static RenderTexture2D target;
static Shader shader;
static bool ready;
static int locResolution;
static int locTime;
static int locBhPos;
static int locBhRadius;
static int locBhStrength;

#if defined(PLATFORM_WEB)
static const char *kFs =
    "#version 100\n"
    "precision mediump float;"
    "varying vec2 fragTexCoord;"
    "uniform sampler2D texture0;"
    "uniform vec2 resolution;"
    "uniform float time;"
    "uniform vec2 bhPos;"
    "uniform float bhRadius;"
    "uniform float bhStrength;"
    "float hash(vec2 p) {"
    "  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);"
    "}"
    "void main() {"
    "  vec2 uv = fragTexCoord;"
    "  vec2 px = vec2(1.6) / resolution;"
    "  vec2 toBh = uv - bhPos;"
    "  float r = length(toBh);"
    "  float lens = bhStrength * exp(-r * r / max(bhRadius * bhRadius * 2.6, 1e-5)) * bhRadius * 0.7;"
    "  lens *= smoothstep(0.0, bhRadius * 0.35, r);"
    "  uv -= (toBh / max(r, 1e-5)) * lens;"
    "  vec2 dir = uv - vec2(0.5);"
    "  float rad = length(dir);"
    "  vec2 off = dir * rad * 0.9 * px;"
    "  off += (toBh / max(r, 1e-5)) * lens * 2.2;"
    "  vec3 col;"
    "  col.r = texture2D(texture0, uv + off).r;"
    "  col.g = texture2D(texture0, uv).g;"
    "  col.b = texture2D(texture0, uv - off).b;"
    "  vec3 acc = vec3(0.0);"
    "  acc += max(texture2D(texture0, uv + vec2( px.x * 3.0, 0.0)).rgb - 0.46, 0.0);"
    "  acc += max(texture2D(texture0, uv + vec2(-px.x * 3.0, 0.0)).rgb - 0.46, 0.0);"
    "  acc += max(texture2D(texture0, uv + vec2(0.0,  px.y * 3.0)).rgb - 0.46, 0.0);"
    "  acc += max(texture2D(texture0, uv + vec2(0.0, -px.y * 3.0)).rgb - 0.46, 0.0);"
    "  acc += max(texture2D(texture0, uv + vec2( px.x * 2.0,  px.y * 2.0)).rgb - 0.50, 0.0);"
    "  acc += max(texture2D(texture0, uv + vec2(-px.x * 2.0, -px.y * 2.0)).rgb - 0.50, 0.0);"
    "  col += acc * 0.22;"
    "  col = col * vec3(1.06, 0.92, 1.22) + vec3(0.020, 0.004, 0.052);"
    "  vec3 shadows = vec3(0.10, 0.03, 0.16);"
    "  vec3 mids = vec3(0.94, 1.04, 1.06);"
    "  float luma = dot(col, vec3(0.299, 0.587, 0.114));"
    "  col += shadows * (1.0 - smoothstep(0.0, 0.45, luma));"
    "  col *= mix(vec3(1.0), mids, smoothstep(0.15, 0.8, luma));"
    "  float hi = smoothstep(0.55, 1.0, luma);"
    "  col += vec3(0.06, 0.03, 0.0) * hi;"
    "  col = col * (1.0 + 2.2 * luma) / (1.0 + luma * 1.9);"
    "  vec2 p = uv * 2.0 - 1.0;"
    "  col *= 1.0 - dot(p, p) * 0.16;"
    "  col += (hash(uv * resolution + fract(time) * 91.7) - 0.5) * 0.028;"
    "  gl_FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);"
    "}";
#else
static const char *kVs =
    "#version 330\n"
    "in vec3 vertexPosition;"
    "in vec2 vertexTexCoord;"
    "out vec2 fragTexCoord;"
    "uniform mat4 mvp;"
    "void main() {"
    "  fragTexCoord = vertexTexCoord;"
    "  gl_Position = mvp * vec4(vertexPosition, 1.0);"
    "}";
static const char *kFs =
    "#version 330\n"
    "in vec2 fragTexCoord;"
    "out vec4 finalColor;"
    "uniform sampler2D texture0;"
    "uniform vec2 resolution;"
    "uniform float time;"
    "uniform vec2 bhPos;"
    "uniform float bhRadius;"
    "uniform float bhStrength;"
    "float hash(vec2 p) {"
    "  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);"
    "}"
    "void main() {"
    "  vec2 uv = fragTexCoord;"
    "  vec2 px = vec2(1.6) / max(resolution, vec2(1.0));"
    "  vec2 toBh = uv - bhPos;"
    "  float r = length(toBh);"
    "  float lens = bhStrength * exp(-r * r / max(bhRadius * bhRadius * 2.6, 1e-5)) * bhRadius * 0.7;"
    "  lens *= smoothstep(0.0, bhRadius * 0.35, r);"
    "  uv -= (toBh / max(r, 1e-5)) * lens;"
    "  vec2 dir = uv - vec2(0.5);"
    "  float rad = length(dir);"
    "  vec2 off = dir * rad * 0.9 * px;"
    "  off += (toBh / max(r, 1e-5)) * lens * 2.2;"
    "  vec3 col;"
    "  col.r = texture(texture0, uv + off).r;"
    "  col.g = texture(texture0, uv).g;"
    "  col.b = texture(texture0, uv - off).b;"
    "  vec3 acc = vec3(0.0);"
    "  acc += max(texture(texture0, uv + vec2( px.x * 3.0, 0.0)).rgb - 0.46, 0.0);"
    "  acc += max(texture(texture0, uv + vec2(-px.x * 3.0, 0.0)).rgb - 0.46, 0.0);"
    "  acc += max(texture(texture0, uv + vec2(0.0,  px.y * 3.0)).rgb - 0.46, 0.0);"
    "  acc += max(texture(texture0, uv + vec2(0.0, -px.y * 3.0)).rgb - 0.46, 0.0);"
    "  acc += max(texture(texture0, uv + vec2( px.x * 2.0,  px.y * 2.0)).rgb - 0.50, 0.0);"
    "  acc += max(texture(texture0, uv + vec2(-px.x * 2.0, -px.y * 2.0)).rgb - 0.50, 0.0);"
    "  acc += max(texture(texture0, uv + vec2( px.x * 5.0, 0.0)).rgb - 0.55, 0.0);"
    "  acc += max(texture(texture0, uv + vec2(-px.x * 5.0, 0.0)).rgb - 0.55, 0.0);"
    "  col += acc * 0.20;"
    "  col = col * vec3(1.06, 0.92, 1.22) + vec3(0.020, 0.004, 0.052);"
    "  vec3 shadows = vec3(0.10, 0.03, 0.16);"
    "  vec3 mids = vec3(0.94, 1.04, 1.06);"
    "  float luma = dot(col, vec3(0.299, 0.587, 0.114));"
    "  col += shadows * (1.0 - smoothstep(0.0, 0.45, luma));"
    "  col *= mix(vec3(1.0), mids, smoothstep(0.15, 0.8, luma));"
    "  float hi = smoothstep(0.55, 1.0, luma);"
    "  col += vec3(0.06, 0.03, 0.0) * hi;"
    "  col = col * (1.0 + 2.2 * luma) / (1.0 + luma * 1.9);"
    "  vec2 p = uv * 2.0 - 1.0;"
    "  col *= 1.0 - dot(p, p) * 0.16;"
    "  col += (hash(uv * resolution + fract(time) * 91.7) - 0.5) * 0.028;"
    "  finalColor = vec4(clamp(col, 0.0, 1.0), 1.0);"
    "}";
#endif

static void EnsureTarget(void) {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (ready && target.texture.width == w && target.texture.height == h) return;
    if (ready) UnloadRenderTexture(target);
    target = LoadRenderTexture(w, h);
    ready = true;
}

void PostFx_Init(void) {
#if defined(PLATFORM_WEB)
    shader = LoadShaderFromMemory(0, kFs);
#else
    shader = LoadShaderFromMemory(kVs, kFs);
#endif
    locResolution = GetShaderLocation(shader, "resolution");
    locTime = GetShaderLocation(shader, "time");
    locBhPos = GetShaderLocation(shader, "bhPos");
    locBhRadius = GetShaderLocation(shader, "bhRadius");
    locBhStrength = GetShaderLocation(shader, "bhStrength");
    EnsureTarget();
}

void PostFx_Shutdown(void) {
    if (ready) UnloadRenderTexture(target);
    UnloadShader(shader);
    ready = false;
}

void PostFx_BeginScene(void) {
    EnsureTarget();
    BeginTextureMode(target);
}

void PostFx_EndScene(Camera camera) {
    EndTextureMode();

    float res[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };
    float t = (float)GetTime();
    BlackHoleScreen bh = BlackHole_GetScreenState(camera);
    float bhPos[2] = { bh.center.x, bh.center.y };
    float radius = bh.visible ? bh.radius * 1.35f : 0.0001f;
    float strength = bh.visible ? 1.0f : 0.0f;

    SetShaderValue(shader, locResolution, res, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, locTime, &t, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, locBhPos, bhPos, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, locBhRadius, &radius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, locBhStrength, &strength, SHADER_UNIFORM_FLOAT);

    BeginShaderMode(shader);
    DrawTextureRec(target.texture,
                   (Rectangle){ 0, 0, (float)target.texture.width, (float)-target.texture.height },
                   (Vector2){ 0, 0 }, WHITE);
    EndShaderMode();
}
