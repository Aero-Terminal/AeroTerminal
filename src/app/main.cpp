#include "WindowFrame.h"
#include <ixwebsocket/IXNetSystem.h>
#include <fstream>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

bool FileExists(const std::string& path) {
    std::ifstream f(path.c_str());
    return f.good();
}

void FixWorkingDirectory() {
    // 1. Check if storage/tldraw_index.html exists in current directory
    if (FileExists("storage/tldraw_index.html")) {
        return;
    }
    
    // 2. Check if storage/tldraw_index.html exists in parent directory
    if (FileExists("../storage/tldraw_index.html")) {
        if (chdir("..") == 0) {
            std::cout << "[AeroTerminal] Changed working directory to parent (project root)" << std::endl;
            return;
        }
    }
    
    // 3. Check relative to executable path
#ifdef _WIN32
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) > 0) {
        std::wstring ws(exePath);
        size_t last_slash = ws.find_last_of(L"\\/");
        if (last_slash != std::wstring::npos) {
            std::wstring exeDir = ws.substr(0, last_slash);
            std::wstring targetFile = exeDir + L"\\storage\\tldraw_index.html";
            DWORD attrib = GetFileAttributesW(targetFile.c_str());
            if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
                if (_wchdir(exeDir.c_str()) == 0) {
                    std::wcout << L"[AeroTerminal] Changed working directory to executable directory: " << exeDir << std::endl;
                    return;
                }
            }
        }
    }
#endif
}

int main()
{
    FixWorkingDirectory();
    ix::initNetSystem();

    quantum::WindowFrame app;
    if (!app.Initialize(1280, 800, "AeroTerminal - Trading Workstation")) {
        ix::uninitNetSystem();
        return 1;
    }
    
    app.Run();
    
    ix::uninitNetSystem();
    return 0;
}
