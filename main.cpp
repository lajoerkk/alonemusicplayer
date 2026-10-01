#include <windows.h>
#include <stdio.h>

typedef void* (__stdcall *AIL_open_stream_t)(void*, const char*, int);
typedef void  (__stdcall *AIL_set_stream_loop_count_t)(void*, int);
typedef void  (__stdcall *AIL_set_stream_volume_t)(void*, int);
typedef void  (__stdcall *AIL_start_stream_t)(void*);
typedef void  (__stdcall *AIL_pause_stream_t)(void*, int);

void Log(const char* text)
{
    FILE* file = fopen("AloneMusicPlayer.log", "a");

    if (file)
    {
        fprintf(file, "%s\n", text);
        fclose(file);
    }
}

DWORD WINAPI PluginThread(LPVOID)
{
    Log("AloneMusicPlayer started.");

    volatile DWORD* milesDriver =
        reinterpret_cast<volatile DWORD*>(0x8F1A24);

    volatile DWORD* playerPed =
        reinterpret_cast<volatile DWORD*>(0x9412F0);

    char musicFile[MAX_PATH] = {0};

    GetPrivateProfileStringA(
        "AloneMusicPlayer",
        "File",
        "",
        musicFile,
        MAX_PATH,
        ".\\AloneMusicPlayer.ini"
    );

    if (musicFile[0] == '\0')
    {
        Log("ERROR: No File entry found in AloneMusicPlayer.ini.");
        return 0;
    }

    int loop =
        GetPrivateProfileIntA(
            "AloneMusicPlayer",
            "Loop",
            1,
            ".\\AloneMusicPlayer.ini"
        );

    int volume =
        GetPrivateProfileIntA(
            "AloneMusicPlayer",
            "Volume",
            127,
            ".\\AloneMusicPlayer.ini"
        );

    int toggleEnabled =
        GetPrivateProfileIntA(
            "AloneMusicPlayer",
            "ToggleEnabled",
            1,
            ".\\AloneMusicPlayer.ini"
        );

    int startInMenu =
        GetPrivateProfileIntA(
            "AloneMusicPlayer",
            "StartInMenu",
            0,
            ".\\AloneMusicPlayer.ini"
        );

    int startPaused =
        GetPrivateProfileIntA(
            "AloneMusicPlayer",
            "StartPaused",
            0,
            ".\\AloneMusicPlayer.ini"
        );

    Log("Configuration loaded.");

    while (*milesDriver == 0)
        Sleep(100);

    HMODULE miles = GetModuleHandleA("Mss32.dll");

    if (!miles)
    {
        Log("ERROR: Mss32.dll was not found.");
        return 0;
    }

    AIL_open_stream_t openStream =
        reinterpret_cast<AIL_open_stream_t>(
            GetProcAddress(miles, "_AIL_open_stream@12"));

    AIL_set_stream_loop_count_t setLoopCount =
        reinterpret_cast<AIL_set_stream_loop_count_t>(
            GetProcAddress(miles, "_AIL_set_stream_loop_count@8"));

    AIL_set_stream_volume_t setVolume =
        reinterpret_cast<AIL_set_stream_volume_t>(
            GetProcAddress(miles, "_AIL_set_stream_volume@8"));

    AIL_start_stream_t startStream =
        reinterpret_cast<AIL_start_stream_t>(
            GetProcAddress(miles, "_AIL_start_stream@4"));

    AIL_pause_stream_t pauseStream =
        reinterpret_cast<AIL_pause_stream_t>(
            GetProcAddress(miles, "_AIL_pause_stream@8"));

    if (!openStream ||
        !setLoopCount ||
        !setVolume ||
        !startStream ||
        !pauseStream)
    {
        Log("ERROR: Required Miles Sound System functions were not found.");
        return 0;
    }

    void* driver =
        reinterpret_cast<void*>(*milesDriver);

    void* stream =
        openStream(
            driver,
            musicFile,
            0
        );

    if (!stream)
    {
        Log("ERROR: Miles could not open the configured audio file.");
        return 0;
    }

    Log("Audio stream opened successfully.");

    setVolume(stream, volume);

    if (loop)
        setLoopCount(stream, 0);
    else
        setLoopCount(stream, 1);

    if (!startInMenu)
    {
        Log("Waiting for gameplay.");

        while (*playerPed == 0)
            Sleep(100);
    }

    bool started = (startPaused == 0);
    bool paused = (startPaused != 0);

    if (started)
    {
        startStream(stream);
        Log("Playback started.");
    }
    else
    {
        Log("Playback waiting in paused state.");
    }

    bool mWasDown = false;

    while (true)
    {
        if (toggleEnabled)
        {
            bool mIsDown =
                (GetAsyncKeyState('M') & 0x8000) != 0;

            if (mIsDown && !mWasDown)
            {
                if (!started)
                {
                    startStream(stream);
                    started = true;
                    paused = false;
                    Log("Playback started.");
                }
                else
                {
                    paused = !paused;
                    pauseStream(stream, paused ? 1 : 0);

                    if (paused)
                        Log("Playback paused.");
                    else
                        Log("Playback resumed.");
                }
            }

            mWasDown = mIsDown;
        }

        Sleep(10);
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        HANDLE thread =
            CreateThread(
                nullptr,
                0,
                PluginThread,
                nullptr,
                0,
                nullptr
            );

        if (thread)
            CloseHandle(thread);
    }

    return TRUE;
}
