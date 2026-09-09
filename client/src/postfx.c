#include "postfx.h"
#include "raylib.h"
#include "rlgl.h"

static RenderTexture2D target;
static Shader shader;
static bool ready;
static int locResolution;

#if defined(PLATFORM_WEB)
static const char *kFs =
    "#version 100\n"
    "precision mediump float;"
    "varying vec2 fragTexCoord;"
    "uniform sampler2D texture0;"
    "uniform vec2 resolution;"
    "void main() {"
    "  vec2 uv = fragTexCoord;"
    "  vec2 px = vec2(1.6) / resolution;"
    "  vec3 col;"
    "  col.r = texture2D(texture0, uv + vec2(px.x, 0.0)).r;"
    "  col.g = texture2D(texture0, uv).g;"
    "  col.b = texture2D(texture0, uv - vec2(px.x, 0.0)).b;"
    "  vec3 acc = vec3(0.0);"
    "  acc += max(texture2D(texture0, uv + vec2( px.x * 3.0, 0.0)).rgb - 0.48, 0.0);"
    "  acc += max(texture2D(texture0, uv + vec2(-px.x * 3.0, 0.0)).rgb - 0.48, 0.0);"
    "  acc += max(texture2D(texture0, uv + vec2(0.0,  px.y * 3.0)).rgb - 0.48, 0.0);"
    "  acc += max(texture2D(texture0, uv + vec2(0.0, -px.y * 3.0)).rgb - 0.48, 0.0);"
    "  col += acc * 0.22;"
    "  col = col * vec3(1.08, 0.90, 1.18) + vec3(0.025, 0.0, 0.05);"
    "  vec2 p = uv * 2.0 - 1.0;"
    "  col *= 1.0 - dot(p, p) * 0.16;"
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
    "void main() {"
    "  vec2 uv = fragTexCoord;"
    "  vec2 px = vec2(1.8) / max(resolution, vec2(1.0));"
    "  vec3 col;"
    "  col.r = texture(texture0, uv + vec2(px.x, 0.0)).r;"
    "  col.g = texture(texture0, uv).g;"
    "  col.b = texture(texture0, uv - vec2(px.x, 0.0)).b;"
    "  vec3 acc = vec3(0.0);"
    "  acc += max(texture(texture0, uv + vec2( px.x * 4.0, 0.0)).rgb - 0.46, 0.0);"
    "  acc += max(texture(texture0, uv + vec2(-px.x * 4.0, 0.0)).rgb - 0.46, 0.0);"
    "  acc += max(texture(texture0, uv + vec2(0.0,  px.y * 4.0)).rgb - 0.46, 0.0);"
    "  acc += max(texture(texture0, uv + vec2(0.0, -px.y * 4.0)).rgb - 0.46, 0.0);"
    "  acc += max(texture(texture0, uv + vec2( px.x * 2.0,  px.y * 2.0)).rgb - 0.50, 0.0);"
    "  acc += max(texture(texture0, uv + vec2(-px.x * 2.0, -px.y * 2.0)).rgb - 0.50, 0.0);"
    "  col += acc * 0.20;"
    "  col = col * vec3(1.10, 0.88, 1.20) + vec3(0.03, 0.0, 0.055);"
    "  vec2 p = uv * 2.0 - 1.0;"
    "  col *= 1.0 - dot(p, p) * 0.17;"
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

void PostFx_EndScene(void) {
    EndTextureMode();
    float res[2] = {(float)GetScreenWidth(), (float)GetScreenHeight()};
    SetShaderValue(shader, locResolution, res, SHADER_UNIFORM_VEC2);
    BeginShaderMode(shader);
    DrawTextureRec(target.texture,
                   (Rectangle){0, 0, (float)target.texture.width, (float)-target.texture.height},
                   (Vector2){0, 0}, WHITE);
    EndShaderMode();
}
