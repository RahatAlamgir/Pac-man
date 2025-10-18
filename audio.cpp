// audio.cpp
// Drop-in replacement that uses WinMM on Windows when miniaudio.h is not present.
// Keeps the same API so you can later switch back to miniaudio by defining USE_MINIAUDIO
// and adding miniaudio.h.

#if defined(USE_MINIAUDIO)

// ---------- Miniaudio path (requires miniaudio.h to be present) ----------
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

static ma_engine g_engine;
static bool g_audio_ok = false;

bool audio_init()
{
    if (ma_engine_init(nullptr, &g_engine) == MA_SUCCESS)
    {
        g_audio_ok = true;
    }
    return g_audio_ok;
}
void audio_shutdown()
{
    if (g_audio_ok)
    {
        ma_engine_uninit(&g_engine);
        g_audio_ok = false;
    }
}
void audio_play(const char *path)
{
    if (!g_audio_ok || !path || !*path)
        return;
    ma_engine_play_sound(&g_engine, path, nullptr);
}

#else

// ---------- Fallback path ----------
// On Windows, use WinMM PlaySound (no extra headers/files).
// On non-Windows, compile to no-op (still links & runs).

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
// Linker hint for MSVC; MinGW/Code::Blocks: also add -lwinmm in linker flags.
#pragma comment(lib, "winmm.lib")
bool audio_init() { return true; }
void audio_shutdown() {}
void audio_play(const char *path)
{
    if (!path || !*path)
        return;
    // ASYNC so it doesn’t block the game loop.
    PlaySoundA(path, NULL, SND_FILENAME | SND_ASYNC);
}
#else
#include <cstdio>
bool audio_init()
{
    std::fprintf(stderr, "[audio] Disabled (no backend)\n");
    return false;
}
void audio_shutdown() {}
void audio_play(const char *) {}
#endif

#endif
