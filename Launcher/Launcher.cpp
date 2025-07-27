#include <windows.h>
#include <shlwapi.h>
#include <commdlg.h>
#include <iostream>
#include <string>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comdlg32.lib")

bool InjectDll(HANDLE hProcess, const std::wstring& dllPath) {
    size_t size = (dllPath.length() + 1) * sizeof(wchar_t);
    LPVOID remoteMem = VirtualAllocEx(hProcess, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) return false;
    if (!WriteProcessMemory(hProcess, remoteMem, dllPath.c_str(), size, nullptr)) {
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        return false;
    }
    LPTHREAD_START_ROUTINE loadLibrary = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (!loadLibrary) return false;
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, loadLibrary, remoteMem, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        return false;
    }
    WaitForSingleObject(hThread, INFINITE);
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hThread);
    return true;
}

std::wstring OpenGameFileDialog() {
    wchar_t filePath[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.lpstrFilter = L"原神游戏本体 (YuanShen.exe;GenshinImpact.exe)\0YuanShen.exe;GenshinImpact.exe\0所有文件\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"请选择 GenshinImpact 或 YuanShen.exe";
    if (GetOpenFileNameW(&ofn)) {
        return filePath;
    }
    return L"";
}

int wmain() {
    std::locale::global(std::locale("zh_CN.UTF-8"));
    std::wcout.imbue(std::locale("zh_CN.UTF-8"));
    wchar_t launcherExePath[MAX_PATH];
    GetModuleFileNameW(nullptr, launcherExePath, MAX_PATH);
    std::wstring launcherPath = launcherExePath;
    std::wstring launcherDir = launcherPath.substr(0, launcherPath.find_last_of(L"\\/"));
    std::wstring iniPath = launcherPath.substr(0, launcherPath.find_last_of(L'.')) + L".ini";
    wchar_t gameExePath[MAX_PATH] = { 0 };
    GetPrivateProfileStringW(L"Settings", L"GamePath", L"", gameExePath, MAX_PATH, iniPath.c_str());
    std::wstring gamePath = gameExePath;
    if (gamePath.empty() || !PathFileExistsW(gamePath.c_str())) {
        std::wcout << L"[+] 请选择游戏文件..." << std::endl;
        gamePath = OpenGameFileDialog();
        if (gamePath.empty()) {
            std::wcerr << L"[-] 用户取消选择" << std::endl;
            system("pause");
            return 1;
        }
        WritePrivateProfileStringW(L"Settings", L"GamePath", gamePath.c_str(), iniPath.c_str());
    }
    std::wstring workingDir = gamePath.substr(0, gamePath.find_last_of(L"\\/"));
    std::wstring dllPath = launcherDir + L"\\Genshin.Fps.UnlockerIsland.dll";
    if (!PathFileExistsW(dllPath.c_str())) {
        std::wcerr << L"[-] 找不到 Genshin.Fps.UnlockerIsland.dll 文件,请确保文件完整" << std::endl;
        system("pause");
        return 1;
    }
    std::wcout << L"[+] 游戏路径: " << gamePath << std::endl;
    std::wcout << L"[+] DLL路径: " << dllPath << std::endl;
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(
        gamePath.c_str(),
        nullptr,
        nullptr,
        nullptr,
        FALSE,
        CREATE_SUSPENDED,
        nullptr,
        workingDir.c_str(),
        &si,
        &pi))
    {
        std::wcerr << L"[-] 无法创建游戏进程: " << gamePath << std::endl;
        system("pause");
        return 1;
    }
    if (!InjectDll(pi.hProcess, dllPath)) {
        std::wcerr << L"[-] 注入 DLL 失败。" << std::endl;
        TerminateProcess(pi.hProcess, 1);
        system("pause");
        return 1;
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    std::wcout << L"[+] 游戏已启动并成功注入" << std::endl;
    return 0;
}
