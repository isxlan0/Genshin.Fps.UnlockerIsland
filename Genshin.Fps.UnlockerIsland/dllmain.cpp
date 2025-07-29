#include "pch.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct Menu_T
{
    bool showGui = true;
    bool show_fps = true;

    enum class FpsCorner
    {
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    int selected_fps = 60;
    int fps_index = 2;
    float fov_value = 90.0f;
    bool enable_fps_override = false;
    bool enable_fov_override = false;
    bool enable_display_fog_override = false;
    bool enable_Perspective_override = false;

};
std::vector<Menu_T::FpsCorner> selected_corners = { Menu_T::FpsCorner::TopLeft };

extern Menu_T menu;

Menu_T menu = {};

namespace Gui
{
    typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
    typedef HRESULT(__stdcall* ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    WNDPROC oWndProc = nullptr;
    Present_t oPresent = nullptr;
    ResizeBuffers_t oResizeBuffers = nullptr;

    HWND g_hWnd = nullptr;
    ID3D11Device* g_pd3dDevice = nullptr;
    ID3D11DeviceContext* g_pd3dContext = nullptr;
    ID3D11RenderTargetView* g_mainRTV = nullptr;
    bool g_ImGuiInitialized = false;

    void CleanupRenderTarget()
    {
        if (g_mainRTV) {
            g_mainRTV->Release();
            g_mainRTV = nullptr;
        }
    }

    void CreateRenderTarget(IDXGISwapChain* pSwapChain)
    {
        ID3D11Texture2D* pBackBuffer = nullptr;
        if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer))) {
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRTV);
            pBackBuffer->Release();
        }
    }

    void SetNiceStyle()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 10.0f;
        style.FrameRounding = 5.0f;
        style.GrabRounding = 5.0f;
        style.ScrollbarRounding = 5.0f;

        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.15f, 0.95f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.45f, 0.70f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.6f, 0.85f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.1f, 0.35f, 0.55f, 1.0f);
    }


    void InitImGui(IDXGISwapChain* pSwapChain)
    {
        if (g_ImGuiInitialized) return;

        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice))) {
            g_pd3dDevice->GetImmediateContext(&g_pd3dContext);

            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            g_hWnd = sd.OutputWindow;

            CreateRenderTarget(pSwapChain);

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;

            ImVector<ImWchar> ranges;
            ImFontGlyphRangesBuilder builder;

            builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
            builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
            builder.BuildRanges(&ranges);

            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyhbd.ttc", 20.0f, NULL, ranges.Data);
            io.Fonts->Build();

            ImGui::StyleColorsDark();
            SetNiceStyle();

            ImGui_ImplWin32_Init(g_hWnd);
            ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

            oWndProc = (WNDPROC)SetWindowLongPtr(sd.OutputWindow, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);


            g_ImGuiInitialized = true;
        }
    }

    const char* GetCornerName(Menu_T::FpsCorner corner)
    {
        switch (corner)
        {
        case Menu_T::FpsCorner::TopLeft: return u8"左上角";
        case Menu_T::FpsCorner::TopRight: return u8"右上角";
        case Menu_T::FpsCorner::BottomLeft: return u8"左下角";
        case Menu_T::FpsCorner::BottomRight: return u8"右下角";
        default: return "Unknown";
        }
    }

    ImVec2 GetCornerPos(Menu_T::FpsCorner corner, const ImVec2& text_size)
    {
        ImVec2 pos;
        auto display_size = ImGui::GetIO().DisplaySize;

        switch (corner)
        {
        case Menu_T::FpsCorner::TopLeft:
            pos = ImVec2(5.f, 5.f);
            break;
        case Menu_T::FpsCorner::TopRight:
            pos = ImVec2(display_size.x - text_size.x - 5.f, 5.f);
            break;
        case Menu_T::FpsCorner::BottomLeft:
            pos = ImVec2(5.f, display_size.y - text_size.y - 5.f);
            break;
        case Menu_T::FpsCorner::BottomRight:
            pos = ImVec2(display_size.x - text_size.x - 5.f, display_size.y - text_size.y - 5.f);
            break;
        }
        return pos;
    }

    // 保存
    void MenuSaveConfig(const char* filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) return;

        file.write(reinterpret_cast<const char*>(&menu), sizeof(menu));

        // 保存 vector 长度
        size_t count = selected_corners.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));

        // 保存 vector 内容
        for (auto& corner : selected_corners) {
            int val = static_cast<int>(corner);
            file.write(reinterpret_cast<const char*>(&val), sizeof(val));
        }

        file.close();
    }


    // 加载
    void MenuLoadConfig(const char* filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) return;

        file.read(reinterpret_cast<char*>(&menu), sizeof(menu));

        // 读取 vector 长度
        size_t count = 0;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));

        selected_corners.clear();
        for (size_t i = 0; i < count; ++i) {
            int val = 0;
            file.read(reinterpret_cast<char*>(&val), sizeof(val));
            selected_corners.push_back(static_cast<Menu_T::FpsCorner>(val));
        }

        file.close();
    }


    CHAR RenderBuff[4096] = { 0 };
    HRESULT __stdcall HookedPresent(IDXGISwapChain* pSwapChain, UINT sync, UINT flags)
    {
        if (!g_ImGuiInitialized)
            InitImGui(pSwapChain);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (menu.show_fps)
        {
            char buffer[64];
            sprintf(buffer, "FPS: %.0f", ImGui::GetIO().Framerate);

            ImDrawList* draw_list = ImGui::GetForegroundDrawList();
            ImVec2 text_size = ImGui::CalcTextSize(buffer);

            for (const auto& corner : selected_corners)
            {
                ImVec2 pos = GetCornerPos(corner, text_size);
                draw_list->AddText(pos, IM_COL32_WHITE, buffer);
            }
        }

        if (menu.showGui)
        {

            ImGui::SetNextWindowSize(ImVec2(300, 660), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);

            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoTitleBar
                | ImGuiWindowFlags_NoNav
                | ImGuiWindowFlags_NoBringToFrontOnFocus;

            ImGui::Begin("MainWindow", nullptr, window_flags);
            {
                ImVec2 window_size = ImGui::GetWindowSize();
                ImGui::SetCursorPosX((window_size.x - ImGui::CalcTextSize(u8"Game Tools 快捷键[Home]").x) * 0.5f);
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), u8"Game Tools 快捷键[Home]");
                ImGui::Separator();
            }

            ImGui::Spacing(); ImGui::Spacing();

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

            if (ImGui::CollapsingHeader(u8"视觉设置", flags))
            {
                static const char* fps_options[] = {
                    "30", "45", "60", "90", "120", "144", "240", "360", "480",
                    "600", "720", "840", "960", "1080", "2147483647"
                };
                ImGui::Checkbox(u8"启用帧数调节", &menu.enable_fps_override);
                ImGui::Text(u8"帧数选择:");
                ImGui::SameLine();
                if (ImGui::BeginCombo("##fps_combo", fps_options[menu.fps_index]))
                {
                    for (int i = 0; i < IM_ARRAYSIZE(fps_options); ++i)
                    {
                        bool is_selected = (menu.fps_index == i);
                        if (ImGui::Selectable(fps_options[i], is_selected))
                        {
                            menu.fps_index = i;
                            menu.selected_fps = std::atoi(fps_options[i]);
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                ImGui::DragInt(u8"帧数数值", &menu.selected_fps, 1, 1, 1000, u8"%d FPS");

                ImGui::Checkbox(u8"启用视角 FOV 修改", &menu.enable_fov_override);
                ImGui::DragFloat(u8"视角 FOV", &menu.fov_value, 0.1f, 30.0f, 150.0f, u8"%.1f°");

                ImGui::Checkbox(u8"启用去雾霾", &menu.enable_display_fog_override);

                ImGui::Checkbox(u8"启用去虚化", &menu.enable_Perspective_override);
            }

            if (ImGui::CollapsingHeader(u8"帧数显示", flags))
            {
                ImGui::Checkbox(u8"显示 FPS", &menu.show_fps);

                ImGui::Text(u8"显示位置:");
                ImGui::Indent();
                for (int i = 0; i < 4; ++i)
                {
                    bool selected = std::find(selected_corners.begin(), selected_corners.end(), static_cast<Menu_T::FpsCorner>(i)) != selected_corners.end();
                    bool checkbox = selected;
                    if (ImGui::Checkbox(GetCornerName(static_cast<Menu_T::FpsCorner>(i)), &checkbox))
                    {
                        if (checkbox)
                            selected_corners.push_back(static_cast<Menu_T::FpsCorner>(i));
                        else
                            selected_corners.erase(std::remove(selected_corners.begin(), selected_corners.end(), static_cast<Menu_T::FpsCorner>(i)), selected_corners.end());
                    }
                }
                ImGui::Unindent();
            }

            if (ImGui::CollapsingHeader(u8"设置配置", flags)) {
                if (ImGui::Button(u8"保存设置", ImVec2(138, 40)))
                {
                    MenuSaveConfig("C:\\Users\\Genshin.Fps.UnlockerIsland.bin");
                }
                ImGui::SameLine();
                if (ImGui::Button(u8"加载设置", ImVec2(138, 40)))
                {
                    MenuLoadConfig("C:\\Users\\Genshin.Fps.UnlockerIsland.bin");
                }
            }

            ImGui::Text(u8"本程序理论可以自动适配最新版游戏\n如出现报错请联系作者反馈!\n免费项目!请勿非法盈利!");
            if (ImGui::Selectable(u8"作者:哔哩哔哩-柯莱宝贝", false, ImGuiSelectableFlags_DontClosePopups)) {
                ShellExecuteA(NULL, "open", "https://space.bilibili.com/1831941574", NULL, NULL, SW_SHOWNORMAL);
            }

            ImGui::End();
        }


        ImGui::Render();
        g_pd3dContext->OMSetRenderTargets(1, &g_mainRTV, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        return oPresent(pSwapChain, sync, flags);
    }

    HRESULT __stdcall HookedResizeBuffers(IDXGISwapChain* pSwapChain,
        UINT BufferCount, UINT Width, UINT Height,
        DXGI_FORMAT NewFormat, UINT SwapChainFlags)
    {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        CleanupRenderTarget();

        HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

        CreateRenderTarget(pSwapChain);
        ImGui_ImplDX11_CreateDeviceObjects();

        return hr;
    }

    LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;
        if (msg == WM_KEYDOWN && wParam == VK_HOME)
        {
            menu.showGui = !menu.showGui;
        }

        return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
    }
}

namespace Tool
{


}

namespace GameHook
{
    uintptr_t hGameModule = 0;

    typedef int(*GameUpdate_t)(__int64 a1, const char* a2);
    GameUpdate_t g_original_GameUpdate = nullptr;

    typedef int(*HookGet_FrameCount_t)();
    HookGet_FrameCount_t g_original_HookGet_FrameCount = nullptr;

    typedef int(*Set_FrameCount_t)(int a);
    Set_FrameCount_t g_original_Set_FrameCount = nullptr;

    typedef int(*HookChangeFOV_t)(__int64 a1, float a2);
    HookChangeFOV_t g_original_HookChangeFOV = nullptr;

    typedef int(*HookDisplayFog_t)(__int64 a1, __int64 a2);
    HookDisplayFog_t g_original_HookDisplayFog = nullptr;

    typedef void* (*HookPlayer_Perspective_t)(void* RCX, float Display, void* R8);
    HookPlayer_Perspective_t g_original_Player_Perspective = nullptr;

    bool GameUpdateInit = false;
    __int64 HookGameUpdate(__int64 a1, const char* a2)
    {
        if (!GameUpdateInit)
        {
            GameUpdateInit = true;
        }
  
        if (menu.enable_fps_override)
        {
            g_original_Set_FrameCount(menu.selected_fps);
        }

        return g_original_GameUpdate(a1, a2);
    }

    int HookGet_FrameCount() {
        int ret = g_original_HookGet_FrameCount();
        if (ret >= 60) return 60;
        else if (ret >= 45) return 45;
        else if (ret >= 30) return 30;
    }

    __int64 HookChangeFOV(__int64 a1, float ChangeFovValue)
    {
        if (menu.enable_fov_override)
        {
            ChangeFovValue = menu.fov_value;
        }

        return g_original_HookChangeFOV(a1, ChangeFovValue);
    }

    __declspec(align(16)) static uint8_t g_fakeFogStruct[64];

    __int64 HookDisplayFog(__int64 a1, __int64 a2)
    {
        if (menu.enable_display_fog_override && a2)
        {
            memcpy(g_fakeFogStruct, (void*)a2, sizeof(g_fakeFogStruct));
            g_fakeFogStruct[0] = 0;
            return g_original_HookDisplayFog(a1, (uintptr_t)g_fakeFogStruct);
        }

        return g_original_HookDisplayFog(a1, a2);
    }

    void* HookPlayer_Perspective(void* RCX, float Display, void* R8)
    {
        if (menu.enable_Perspective_override)
        {
            Display = 1.f;
        }
        return g_original_Player_Perspective(RCX, Display, R8);
    }


    bool InitHook()
    {
        hGameModule = (uintptr_t)GetModuleHandleA(NULL);
        while (
            (hGameModule = (uintptr_t)GetModuleHandleA(NULL)) == NULL) {
            Sleep(1000);
        }

        std::cout << "GameModule:" << hGameModule << std::endl;

        void* GameUpdateAddr = (void*)PatternScanner::Scan("E8 ? ? ? ? 48 8D 4C 24 ? 8B F8 FF 15 ? ? ? ? E8 ? ? ? ?");
        GameUpdateAddr = (void*)PatternScanner::ResolveRelativeAddress((uintptr_t)GameUpdateAddr);
        if (!GameUpdateAddr) {
            MessageBoxA(nullptr, "HookGameUpdate search failed!", "PatternScanner", MB_OK | MB_ICONERROR);
        }

        if (!MinHookManager::Add(GameUpdateAddr, &HookGameUpdate, (void**)&g_original_GameUpdate)) {
            MessageBoxA(nullptr, "HookGameUpdate install failed!", "MinHook", MB_OK | MB_ICONERROR);
        }

        void* Get_FrameCountAddr = (void*)PatternScanner::Scan("E8 ? ? ? ? 85 C0 7E 0E E8 ? ? ? ? 0F 57 C0 F3 0F 2A C0 EB 08 ?");
        Get_FrameCountAddr = (void*)PatternScanner::ResolveRelativeAddress((uintptr_t)Get_FrameCountAddr);
        Get_FrameCountAddr = (void*)PatternScanner::ResolveRelativeAddress((uintptr_t)Get_FrameCountAddr);
        if (!Get_FrameCountAddr) {
            MessageBoxA(nullptr, "HookGet_FrameCount search failed!", "PatternScanner", MB_OK | MB_ICONERROR);
        }

        if (!MinHookManager::Add(Get_FrameCountAddr, &HookGet_FrameCount, (void**)&g_original_HookGet_FrameCount)) {
            MessageBoxA(nullptr, "HookGet_FrameCount install failed!", "MinHook", MB_OK | MB_ICONERROR);
        }

        void* Set_FrameCountAddr = (void*)PatternScanner::Scan("E8 ? ? ? ? E8 ? ? ? ? 83 F8 1F 0F 9C 05 ? ? ? ? 48 8B 05 ? ? ? ? ");
        Set_FrameCountAddr = (void*)PatternScanner::ResolveRelativeAddress((uintptr_t)Set_FrameCountAddr);
        Set_FrameCountAddr = (void*)PatternScanner::ResolveRelativeAddress((uintptr_t)Set_FrameCountAddr);
        if (!Set_FrameCountAddr) {
            MessageBoxA(nullptr, "Set_FrameCountAddr search failed!", "PatternScanner", MB_OK | MB_ICONERROR);
        }
        g_original_Set_FrameCount = (Set_FrameCount_t)Set_FrameCountAddr;

        void* ChangeFOVAddr = (void*)PatternScanner::Scan("40 53 48 83 EC 60 0F 29 74 24 ? 48 8B D9 0F 28 F1 E8 ? ? ? ? 48 85 C0 0F 84 ? ? ? ? E8 ? ? ? ? 48 8B C8 ");
        if (!ChangeFOVAddr) {
            MessageBoxA(nullptr, "HookChangeFOV search failed!", "PatternScanner", MB_OK | MB_ICONERROR);
        }
        if (!MinHookManager::Add(ChangeFOVAddr, &HookChangeFOV, (void**)&g_original_HookChangeFOV)) {
            MessageBoxA(nullptr, "HookChangeFOV install failed!", "MinHook", MB_OK | MB_ICONERROR);
        }

        void* DisplayFogAddr = (void*)PatternScanner::Scan("0F B6 02 88 01 8B 42 04 89 41 04 F3 0F 10 52 ? F3 0F 10 4A ? F3 0F 10 42 ? 8B 42 08 ");
        if (!DisplayFogAddr) {
            MessageBoxA(nullptr, "HookDisplayFog search failed!", "PatternScanner", MB_OK | MB_ICONERROR);
        }
        if (!MinHookManager::Add(DisplayFogAddr, &HookDisplayFog, (void**)&g_original_HookDisplayFog)) {
            MessageBoxA(nullptr, "HookDisplayFog install failed!", "MinHook", MB_OK | MB_ICONERROR);
        }

        void* Player_PerspectiveAddr = (void*)PatternScanner::Scan("E8 ? ? ? ? 48 8B BE ? ? ? ? 80 3D ? ? ? ? ? 0F 85 ? ? ? ? 80 BE ? ? ? ? ? 74 11");
        Player_PerspectiveAddr = (void*)PatternScanner::ResolveRelativeAddress((uintptr_t)Player_PerspectiveAddr);
        if (!Player_PerspectiveAddr) {
            MessageBoxA(nullptr, "HookPlayer_Perspective search failed!", "PatternScanner", MB_OK | MB_ICONERROR);
        }
        if (!MinHookManager::Add(Player_PerspectiveAddr, &HookPlayer_Perspective, (void**)&g_original_Player_Perspective)) {
            MessageBoxA(nullptr, "HookPlayer_Perspective install failed!", "MinHook", MB_OK | MB_ICONERROR);
        }

        return true;
    }
}

DWORD WINAPI Run(LPVOID lpParam)
{
    GameHook::InitHook();

    while (!GameHook::GameUpdateInit)
    {
        Sleep(1000);
    }


    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L,
        GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("Genshin.Fps.UnlockerIslandWnd"), NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, _T("Genshin.Fps.UnlockerIsland"), WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    IDXGISwapChain* pSwapChain = nullptr;
    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;

    if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 1,
        D3D11_SDK_VERSION, &sd, &pSwapChain, &pDevice, &level, &pContext)))
    {
        void** vTable = *reinterpret_cast<void***>(pSwapChain);

        MinHookManager::Add(vTable[8], &Gui::HookedPresent, reinterpret_cast<void**>(&Gui::oPresent));// Present
        MinHookManager::Add(vTable[13], &Gui::HookedResizeBuffers, reinterpret_cast<void**>(&Gui::oResizeBuffers));// ResizeBuffers

        pSwapChain->Release();
        pDevice->Release();
        pContext->Release();
    }

    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);


    return TRUE;
}


BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, Run, NULL, 0, NULL);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH)
    {
        MH_Uninitialize();
    }
    return TRUE;
}