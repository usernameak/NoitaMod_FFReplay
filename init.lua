local ffi = require("ffi")

local dll_path = "mods/ffreplay/ffreplay.dll"

ffi.cdef([[
	void ffreplay_init(const char* ffmpegPath);
	void* LoadLibraryA(const char*);
]])

assert(ffi.C.LoadLibraryA(dll_path) ~= nil) -- thanks to Noita-Dear-ImGui dev for this trick

local ffreplay_dll = ffi.load(dll_path)
ffreplay_dll.ffreplay_init("mods/ffreplay/ffmpeg.exe")
