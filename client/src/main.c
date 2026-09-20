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
#include "pocketfx.h"
#include "dragon.h"       /* v65.41: the wandering dragon + gilded hoard */
#include "colonnade.h"
#include "golem.h"   /* v65.8: pocket universe sky swap */
#include "clientlog.h"   /* v65.9.1 */
#include "atlasheal.h"   /* v65.11 */
#include "blackhole.h"
#include "ship.h"
#include "settings.h"
#include "postfx.h"
#include "soundfx.h"
#include "usermusic.h"
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
    ClientLog_Init();   /* v65.9.1: midless_client.log beside the exe */

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
    AtlasHeal_Terrain(&texture);   /* v65.11: patch blank pocket tiles if the
                                    * png predates the pocket universe */
    
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
    Colonnade_Init();      /* v65.16: the pocket peristyle */
    Golem_Init();          /* v65.21: the Warden of the Meadow */
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
    ClientLog_Shutdown();
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
        /* v65.13: the pocket universe keeps its own calm - the moths,
         * the void-tide and its mob waves do not exist inside it. */
        if (PocketFx_FactorAny() < 0.5f) {
            Bird_Update(GetFrameTime());
            Hunter_Update(GetFrameTime());
            Mobs_Update(GetFrameTime());
            Dragon_Update();   /* v65.41: appearances + the 10-minute hoard */
        }
        SoundFx_PocketUpdate(PocketFx_Factor());
        SoundFx_Pocket2Update(PocketFx_Factor2());   /* v65.28: the Foundry station */
        Golem_Update(GetFrameTime());
        Ship_Update((double)GetTime());   /* v62 */
        MapView_Update();
        SoundFx_Update();
    }

    /* v65.35: the user parkour playlist must be pumped every frame -
     * even on pause/menu screens, or the stream starves mid-track */
    UserMusic_Update();

    /* v65.8: pocket universe mood follows the player (menus stay cosmic) */
    PocketFx_Update(inWorld ? player.camera.position : (Vector3){ 0, 0, 0 });

    Vector3 selectionBoxPos = (Vector3) { floor(player.rayResult.hitPos.x), floor(player.rayResult.hitPos.y), floor(player.rayResult.hitPos.z)};

    BeginDrawing();
        ClearBackground(PocketFx_SkyColor());   /* v65.8: day sky in the pocket */

        /* v65.9.1: never let an outdated atlas look like "invisible blocks" */
        if (!Block_PocketTilesPresent()) {
            const char *warn = "OUTDATED textures/terrain.png - extract the WHOLE release zip over the game folder, then restart";
            int width = MeasureText(warn, 20);
            DrawRectangle(0, 0, GetScreenWidth(), 30, (Color){ 120, 20, 20, 220 });
            DrawText(warn, (GetScreenWidth() - width) / 2, 6, 20, (Color){ 255, 230, 230, 255 });
        }

        if (inWorld) {
            PostFx_BeginScene();
            ClearBackground(PocketFx_SkyColor());   /* v65.8: day sky in the pocket */
            BeginMode3D(player.camera);
                Starfield_Update(GetFrameTime());
                /* v65.8: the pocket universe has no void - hide the nebulae,
                 * the black-hole sun and the ship traffic while inside it */
                if (PocketFx_FactorAny() < 0.5f) {
                    Starfield_Draw(player.camera);
                    BlackHole_Draw(player.camera);
                    Ship_Draw((double)GetTime());   /* v62: the Void Runner */
                } else if (PocketFx_ViewZone() == 2) {
                    /* v65.36: the parkour instance is cosmic like the main
                     * game - stars and the black-hole sun are drawn (the
                     * post-FX lensing stays zeroed inside pockets); ship
                     * traffic and overworld threats remain excluded */
                    Starfield_Draw(player.camera);
                    BlackHole_Draw(player.camera);
                }
                World_Draw(player.camera.position);
                World_DrawWireAuras();
                Dragon_Draw(player.camera.position);   /* v65.41: sub-block
                    * .vox dragon circling a random island (main world only -
                    * the module gates itself on the pocket factor) */
                Colonnade_Draw(PocketFx_Factor());   /* v65.16 peristyle */
                Golem_Draw();                        /* v65.21 the Warden */
                /* v65.13: no overworld threats or auras bleed into the pocket */
                if (PocketFx_FactorAny() < 0.5f) {
                    Hunter_Draw();
                    Mobs_Draw();
                }
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
