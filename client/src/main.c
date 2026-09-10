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
#include "settings.h"
#include "postfx.h"
#include "soundfx.h"
#include "mapview.h"
#include "bird.h"
#include "hunter.h"


void Game_RunLoop(void);

int main(void) {
    if (!RuntimePaths_Init()) return 1;

    Settings_Load();
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_ALWAYS_RUN | FLAG_MSAA_4X_HINT);
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
                World_Draw(player.camera.position);
                World_DrawWireAuras();
                Hunter_Draw();
                Player_DrawWeb();
                if (player.cameraMode == PLAYER_CAMERA_FIRST_PERSON) Player_Draw();
                if (player.rayResult.hitblockId != -1) {
                    const Block *block = Block_GetDefinition(player.rayResult.hitblockId);
                    Vector3 blockSize = Vector3Subtract(block->maxBB, block->minBB);
                    blockSize = Vector3Scale(blockSize, 1.0f / 16);
                    selectionBoxPos = Vector3Add(selectionBoxPos,
                        Vector3Scale(Vector3Add(block->minBB, block->maxBB), 1.0f / 32));
                    DrawCube(selectionBoxPos, blockSize.x + 0.02f, blockSize.y + 0.02f, blockSize.z + 0.02f, (Color){255, 255, 255, 40});
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
