#include <cstdint>
#include <string>
#include <windows.h>

// missing typedefs for poro
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

#include "poro/source/poro/irender_texture.h"
#include "poro/source/poro/itexture.h"
#include "poro/source/poro/igraphics.h"
#include "poro/source/poro/iplatform.h"
#include "poro/source/poro/fileio.h"

static poro::IPlatform *g_poroPlatformInstance = reinterpret_cast<poro::IPlatform *>(static_cast<uintptr_t>(0x10306E0));

typedef std::wstring &(__thiscall *PFileSystem_CompleteWritePath)(
    poro::FileSystem *self,
    std::wstring &outStr,
    const poro::FileLocation::Enum location,
    const std::string &path);
static PFileSystem_CompleteWritePath FileSystem_CompleteWritePath =
    reinterpret_cast<PFileSystem_CompleteWritePath>(static_cast<uintptr_t>(0xC3DC90));

typedef void(__cdecl *POpDelete)(void *);
static POpDelete OpDelete = reinterpret_cast<POpDelete>(static_cast<uintptr_t>(0xD41504));

#if 0
struct GifEncoder {
    poro::WriteStream stream;
    char buffer[768];
    uint16_t width;
    uint16_t height;
    uint16_t field_30C;
    uint16_t field_30E;
    int field_310;
    int field_314;
    int field_318;
};
#endif

struct fake_std_wstring {
    union string_buffer {
        wchar_t buf[8];
        wchar_t *ptr;
    };
    string_buffer buffer;
    size_t size;
    size_t capacity;

    void release_buffer() {
        if (capacity >= 16) {
            POpDelete(buffer.ptr);
            buffer.ptr = nullptr;
        }
        size     = 0;
        capacity = 1;
    }
};

std::wstring g_ffmpegPath = L"";

struct FfmpegEncoder {
    HANDLE process;
    HANDLE stdinPipeWrite;
    HANDLE stdinPipeRead;

    uint8_t pad[784];

    FfmpegEncoder()
        : stdinPipeWrite(INVALID_HANDLE_VALUE),
          stdinPipeRead(INVALID_HANDLE_VALUE),
          process(INVALID_HANDLE_VALUE) {
    }

    HANDLE openPipe(const wchar_t *output_path, int width, int height) {
        if (stdinPipeWrite != INVALID_HANDLE_VALUE) return stdinPipeWrite;

        const wchar_t *ffmpeg_path = L"C:\\msys64\\mingw64\\bin\\ffmpeg.exe";

        wchar_t cmdline[512];
        swprintf(cmdline, 512,
            L"\"%s\" -r 30 -f rawvideo -pix_fmt rgba -s %dx%d -i - "
            "-vcodec libx264 -crf 10 -pix_fmt yuv420p -an -vf vflip -y "
            "-preset superfast -tune animation \"%s.mp4\"",
            ffmpeg_path, width, height, output_path);

        SECURITY_ATTRIBUTES sa;
        sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = nullptr;
        sa.bInheritHandle       = TRUE;

        CreatePipe(&stdinPipeRead, &stdinPipeWrite, &sa, 0);

        // do not inherit the write side of the pipe
        SetHandleInformation(stdinPipeWrite, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW sti{};
        sti.cb         = sizeof(sti);
        sti.dwFlags    = STARTF_USESTDHANDLES;
        sti.hStdInput  = stdinPipeRead;
        sti.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        sti.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi{};

        CreateProcessW(
            ffmpeg_path,
            cmdline,
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &sti,
            &pi);
        if (stdinPipeRead != INVALID_HANDLE_VALUE) {
            CloseHandle(stdinPipeRead);
            stdinPipeRead = INVALID_HANDLE_VALUE;
        }

        process = pi.hProcess;
        CloseHandle(pi.hThread);

        return stdinPipeWrite;
    }

    ~FfmpegEncoder() {
        if (stdinPipeWrite != INVALID_HANDLE_VALUE) {
            CloseHandle(stdinPipeWrite);
        }
        if (stdinPipeRead != INVALID_HANDLE_VALUE) {
            CloseHandle(stdinPipeRead);
        }
        if (process != INVALID_HANDLE_VALUE) {
            WaitForSingleObject(process, INFINITE);
            CloseHandle(process);
        }
    }
};

static_assert(sizeof(FfmpegEncoder) == 796);

struct ReplayEncoder {
    int currentFrame;
    // GifEncoder gifEncoder;
    FfmpegEncoder encoder;
    std::string filename;
    double timeElapsed;
    int framesEncoded;
    int numFrames;
    double startTime;

    ReplayEncoder()
        : currentFrame(0),
          encoder{},
          timeElapsed(0),
          framesEncoded(0),
          numFrames(0),
          startTime(0) {}
};

struct ReplayEncoderInput {
    poro::IRenderTexture **frameArrayStart;
    poro::IRenderTexture **frameArrayEnd;
};

ReplayEncoder *__fastcall encodeReplay_hook(
    ReplayEncoder *encoder,
    poro::IGraphics *graphics,

    void *fakearg, // fake argument for LTO hooking

    const std::string &filename,
    ReplayEncoderInput *encoderInput,
    int unknown,
    unsigned int outputScale,
    int cropLeft, int cropTop,
    int cropRight, int cropBottom) {

    if (encoderInput->frameArrayStart == encoderInput->frameArrayEnd)
        return nullptr;

    if (!encoder) {
        encoder            = new ReplayEncoder;
        encoder->numFrames = encoderInput->frameArrayEnd - encoderInput->frameArrayStart;
        encoder->startTime = 0.0;
    }

    int dataWidth  = encoderInput->frameArrayStart[0]->GetTexture()->GetDataWidth();
    int dataHeight = encoderInput->frameArrayStart[0]->GetTexture()->GetDataHeight();

    std::wstring path;
    FileSystem_CompleteWritePath(g_poroPlatformInstance->GetFileSystem(), path, poro::FileLocation::WorkingDirectory, filename);
    HANDLE pipe = encoder->encoder.openPipe(path.c_str(), dataWidth, dataHeight);

    // fake destructor call
    reinterpret_cast<fake_std_wstring *>(&path)->release_buffer();

    uint8_t *buffer = new uint8_t[dataWidth * dataHeight * 4];

    graphics->RenderTexture_ReadTextureDataFromGPU(encoderInput->frameArrayStart[encoder->currentFrame], buffer);

    WriteFile(pipe, buffer, dataWidth * dataHeight * 4, nullptr, nullptr);

    delete[] buffer;

    encoder->currentFrame++;
    encoder->framesEncoded++;

    if (encoder->framesEncoded != encoder->numFrames) {
        return encoder;
    }

    delete encoder;

    return nullptr;
}

void hookFunction(void *addr, void *targetAddr) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    const auto uptr = reinterpret_cast<uintptr_t>(addr);

    auto *alignedAddr = reinterpret_cast<void *>(uptr & ~(si.dwPageSize - 1));

    DWORD oldProtect;
    VirtualProtect(alignedAddr, si.dwPageSize, PAGE_EXECUTE_READWRITE, &oldProtect);

    const auto targetUptr = reinterpret_cast<uintptr_t>(targetAddr);
    auto *pData           = static_cast<uint8_t *>(addr);

    // call targetUptr
    pData[0]                                 = 0xE8;
    *reinterpret_cast<uint32_t *>(&pData[1]) = targetUptr - uptr - 5;

    // sub esp,0x24 - there's weird ABI with caller cleanup
    pData[5] = 0x83;
    pData[6] = 0xEC;
    pData[7] = 0x24;

    // ret
    pData[8] = 0xC3;

    VirtualProtect(alignedAddr, si.dwPageSize, oldProtect, &oldProtect);
}

extern "C" __declspec(dllexport) void ffreplay_init(const char *ffmpegPath) {
    int sz = MultiByteToWideChar(CP_UTF8, 0, ffmpegPath, -1, nullptr, 0);
    g_ffmpegPath.resize(sz - 1);
    MultiByteToWideChar(CP_UTF8, 0, ffmpegPath, -1, g_ffmpegPath.data(), sz);

    hookFunction((void *)(uintptr_t)0x73E680, (void *)&encodeReplay_hook);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
    }
    return TRUE;
}
