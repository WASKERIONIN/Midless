/*
 * lua_syntax_check.c - v65.28 dev tool: compile-check a Lua mod WITHOUT
 * executing it (luaL_loadstring = parse + codegen only). The sandbox has
 * no system lua, but the repo vendors minilua.h, so:
 *
 *   gcc -O0 -Ilibs tools_dev/lua_syntax_check.c -o /tmp/luacheck -lm -ldl
 *   /tmp/luacheck mods/cosmic_islands.lua
 *
 * Keep this tool: it is the only Lua syntax gate before CI.
 */
#define LUA_IMPL
#include "minilua.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s file.lua\n", argv[0]); return 2; }
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) { perror(argv[1]); return 2; }
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *src = (char *)malloc((size_t)n + 1);
    if (!src || fread(src, 1, (size_t)n, fp) != (size_t)n) { fprintf(stderr, "read fail\n"); return 2; }
    src[n] = 0;
    fclose(fp);

    lua_State *L = luaL_newstate();
    if (!L) { fprintf(stderr, "no lua state\n"); return 2; }
    if (luaL_loadstring(L, src) != LUA_OK) {
        fprintf(stderr, "LUA SYNTAX ERROR in %s:\n%s\n", argv[1], lua_tostring(L, -1));
        return 1;
    }
    printf("LUA SYNTAX OK: %s\n", argv[1]);
    return 0;
}
