/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "player.h"
#include "i18n.h"
#include "world.h"
#include "resource.h"
#include "textures.h"
#include "screens.h"
#include "block.h"
#include "networkhandler.h"
#include "chat.h"
#include "localserver.h"
#include "runtimepaths.h"
#include "starfield.h"
#include "blackhole.h"
#include "ship.h"
#include "settings.h"
#include "postfx.h"
#include "soundfx.h"
#include "mapview.h"
#include "bird.h"
#include "hunter.h"
#include "mobs.h"


void Game_RunLoop(void);

int main(void) {
    if (!RuntimePaths_Init()) return 1;

    Settings_Load();
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_ALWAYS_RUN | FLAG_MSAA_4X_HINT |
                   (gameSettings.vsync ? FLAG_VSYNC_HINT : 0));   /* v56 */
    InitWindow(gameSettings.width, gameSettings.height, "Midless: Cosmic Edition");
    Settings_ApplyWindow();
    SetExitKey(0);
    SetTraceLogLevel(LOG_WARNING);

    #if defined(PLATFORM_WEB)
        char *chunkShaderVs = 
            #include "chunk/shaders/chunk_shader_gl100.vs"
        ;
        char *chunkShaderFs = 
            #include "chunk/shaders/chunk_shader_gl100.fs"
        ;
    #else
        char *chunkShaderVs = 
            #include "chunk/shaders/chunk_shader.vs"
        ;
        char *chunkShaderFs = 
            #include "chunk/shaders/chunk_shader.fs"
        ;
    #endif

    Image midlessLogo = Resource_LoadImage("midless.png"); 


    SetWindowIcon(midlessLogo);
    UnloadImage(midlessLogo);

    EntityModelDefinitions_Init();
    Block_BuildDefinition();

    // World Initialization
    World_Init();

    
    Shader shader = LoadShaderFromMemory(chunkShaderVs, chunkShaderFs);
    Texture2D texture = Resource_LoadTexture("terrain.png"); 
    
    World_ApplyTexture(texture);
    World_ApplyShader(shader);

    //Player Initialization
    Player_Init();
    Starfield_Init();
    BlackHole_Init();
    PostFx_Init();
    SoundFx_Init();
    SoundFx_SetVolume(gameSettings.volume / 100.0f);
    MapView_Init();
    Bird_Init();
    Hunter_Init();
    Mobs_Init();
    Ship_Init();   /* v62: sky traffic */
    I18n_SetLanguage(gameSettings.language);   /* v59: loads the UTF-8 font once */
    Player_LoadProgress();
    
    bool exitProgram = false;
    Screen_Init(texture, &exitProgram);
    ClientTextures_Init(texture);


    #if defined(PLATFORM_WEB)
        emscripten_set_main_loop(Game_RunLoop, 0, 1);
    #else
        while (!WindowShouldClose() && !exitProgram) {
            Game_RunLoop();
        }
        
        networkThreadState = -1;

        LocalServer_Stop();
        EntityModel_ResetDefinitions();
        ClientTextures_Reset();
        Screen_Shutdown();
        UnloadShader(shader);
        UnloadTexture(texture);
        World_Shutdown();
        EntityModelDefinitions_Shutdown();
        PostFx_Shutdown();
        BlackHole_Shutdown();
        Starfield_Shutdown();
        Bird_Shutdown();
        Ship_Shutdown();
        MapView_Shutdown();
        SoundFx_Shutdown();
        Chat_Shutdown();

        CloseWindow();
    #endif

    return 0;
}

void Game_RunLoop(void) {
    if (IsKeyPressed(KEY_F11)) Settings_ToggleFullscreen();

    Network_ProcessIncomingPackets();

    bool inWorld = currentScreen == SCREEN_GAME || currentScreen == SCREEN_PAUSE ||
                   currentScreen == SCREEN_OPTIONS;
    if (inWorld) {
        Player_Update();
        World_Update();
        BlackHole_Update(GetFrameTime());
        Bird_Update(GetFrameTime());
        Hunter_Update(GetFrameTime());
        Mobs_Update(GetFrameTime());
        Ship_Update((double)GetTime());   /* v62 */
        MapView_Update();
        SoundFx_Update();
    }

    Vector3 selectionBoxPos = (Vector3) { floor(player.rayResult.hitPos.x), floor(player.rayResult.hitPos.y), floor(player.rayResult.hitPos.z)};

    BeginDrawing();
        ClearBackground((Color){ 14, 4, 28, 255 });

        if (inWorld) {
            PostFx_BeginScene();
            ClearBackground((Color){ 14, 4, 28, 255 });
            BeginMode3D(player.camera);
                Starfield_Update(GetFrameTime());
                Starfield_Draw(player.camera);
                BlackHole_Draw(player.camera);
                Ship_Draw((double)GetTime());   /* v62: the Void Runner */
                World_Draw(player.camera.position);
                World_DrawWireAuras();
                Hunter_Draw();
                Mobs_Draw();
                /* v46.1: web anchor indicator under the crosshair */
                {
                    Vector3 webCell;
                    if (Player_GetWebTarget(&webCell)) {
                        double wt = (double)GetTime();
                        float pulse = 0.55f + 0.45f * sinf(wt * 6.0f);
                        unsigned char bright = (unsigned char)(130.0f + 110.0f * pulse);
                        Color wc = { bright, bright, (unsigned char)(bright + 12 > 255 ? 255 : bright + 12), 255 };
                        Vector3 c0 = webCell;
                        Vector3 c1 = Vector3Add(webCell, (Vector3){ 1, 1, 1 });
                        rlDrawRenderBatchActive();
                        rlBegin(RL_LINES);
                        rlColor4ub(wc.r, wc.g, wc.b, 255);
                        Vector3 corners[8] = {
                            { c0.x, c0.y, c0.z }, { c1.x, c0.y, c0.z },
                            { c1.x, c0.y, c1.z }, { c0.x, c0.y, c1.z },
                            { c0.x, c1.y, c0.z }, { c1.x, c1.y, c0.z },
                            { c1.x, c1.y, c1.z }, { c0.x, c1.y, c1.z }
                        };
                        static const int wedges[12][2] = {
                            {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
                        };
                        for (int e = 0; e < 12; e++) {
                            rlVertex3f(corners[wedges[e][0]].x, corners[wedges[e][0]].y, corners[wedges[e][0]].z);
                            rlVertex3f(corners[wedges[e][1]].x, corners[wedges[e][1]].y, corners[wedges[e][1]].z);
                        }
                        /* center diamond lock marker */
                        {
                            Vector3 ctr = Vector3Scale(Vector3Add(c0, c1), 0.5f);
                            float r = 0.16f + 0.05f * pulse;
                            Vector3 d[4] = {
                                { ctr.x, ctr.y + r, ctr.z }, { ctr.x + r, ctr.y, ctr.z },
                                { ctr.x, ctr.y - r, ctr.z }, { ctr.x - r, ctr.y, ctr.z }
                            };
                            for (int k = 0; k < 4; k++) {
                                rlVertex3f(d[k].x, d[k].y, d[k].z);
                                rlVertex3f(d[(k + 1) % 4].x, d[(k + 1) % 4].y, d[(k + 1) % 4].z);
                            }
                        }
                        rlEnd();
                        rlDrawRenderBatchActive();
                    }
                }
                Player_DrawWeb();
                Player_DrawLaser();
                if (player.cameraMode == PLAYER_CAMERA_FIRST_PERSON) Player_Draw();
                if (player.rayResult.hitblockId != -1) {
                    const Block *block = Block_GetDefinition(player.rayResult.hitblockId);
                    Vector3 blockSize = Vector3Subtract(block->maxBB, block->minBB);
                    blockSize = Vector3Scale(blockSize, 1.0f / 16);
                    selectionBoxPos = Vector3Add(selectionBoxPos,
                        Vector3Scale(Vector3Add(block->minBB, block->maxBB), 1.0f / 32));
                    /* v59: wireframe outline - the old 40-alpha white cube read as a solid bar */
                    DrawCubeWires(selectionBoxPos, blockSize.x + 0.02f, blockSize.y + 0.02f, blockSize.z + 0.02f, (Color){255, 255, 255, 180});
                }
            EndMode3D();

            Color liquidTint;
            if (Player_GetCameraLiquidTint(&liquidTint)) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), liquidTint);
            }
            PostFx_EndScene(player.camera);
        }

        Screen_Draw();
    EndDrawing();
}
