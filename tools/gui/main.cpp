#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <thread>
#include <sstream>
#include <fstream>
#include <atomic>
#include <vector>
#define IGFD_DEFINE_UNICODE
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#define STB_IMAGE_IMPLEMENTATION
#include <ImGuiFileDialog/stb/stb_image.h>
#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#endif
#include <mutex>
#include <osgDB/ConvertUTF>

#define VERSION "osgGISPlugins Tools Desktop v2.3.0"

struct Model23dtilesParams {
#ifdef _WIN32
    char inputModel[1024] = "C:\\";
    char outputDir[1024] = "C:\\";
#else
    char inputModel[1024] = "/";
    char outputDir[1024] = "/";
#endif

    // 坐标参数
    bool useLatLngAlt = true;
    double lat = 30.0, lng = 116.0, alt = 300.0;
    char epsgStr[10] = "";

    // 变换参数
    double tx = 0.0, ty = 0.0, tz = 0.0;
    double sx = 1.0, sy = 1.0, sz = 1.0;
    int upAxis = 1;

    // 树类型
    int treeType = 0;

    // 压缩
    double r = 0.5;
    int tf = 3, vf = 0, cl = 1;

    // 性能限制
    int tri = 200000, dc = 20;
    int tw = 256, th = 256, aw = 2048, ah = 2048;

    // 其他
    bool nft = false, nrm = false, unlit = false, gn = false;
    int nm = 0;

    static constexpr const char* upAxisItems[] = { "X", "Y", "Z" };
    static constexpr const char* treeTypes[] = { "quad", "oc", "kd" };
    static constexpr const char* texFormats[] = { "png", "jpg", "webp", "ktx2" };
    static constexpr const char* vertFormats[] = { "none", "draco", "meshopt", "quantize", "quantize_meshopt" };
    static constexpr const char* clLevels[] = { "low", "medium", "high" };
    static constexpr const char* nmModes[] = { "f (面法线)", "v (顶点法线)" };
};

static std::string conversionLogs[4];
static std::atomic<bool> isConverting = false;
static std::atomic<bool> scrollToBottom = false;
static std::mutex logMutex;
static std::string currentToolName = "";
#ifdef _WIN32
static std::vector<PROCESS_INFORMATION> pis = {};
#endif // _WIN32
static Model23dtilesParams modelParams;

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_CLOSE_EVENT || signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT ||
        signal == CTRL_LOGOFF_EVENT || signal == CTRL_SHUTDOWN_EVENT) {

        for (auto& pi : pis) {
            if (pi.hProcess != NULL) {
                DWORD pid = pi.dwProcessId;
                std::string cmd = "taskkill /PID " + std::to_string(pid) + " /F";
                system(cmd.c_str());
            }
        }

        pis.clear();
    }
    return TRUE;
}
void closeConsoleWindow() {
    HWND hWnd = GetConsoleWindow();
    if (hWnd != NULL) {
        // 关闭控制台窗口
        PostMessage(hWnd, WM_CLOSE, 0, 0);
    }
}

void onWindowClose(GLFWwindow* window) {
    for (auto& pi : pis) {
        if (pi.hProcess != NULL) {
            //提示用户当前有子进程正在运行，确认是否退出程序
        }
    }
    closeConsoleWindow();
    glfwSetWindowShouldClose(window, GLFW_TRUE); // 标记窗口关闭
}
void openURL(const char* url) {
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}
#else
void openURL(const char* url) {
    std::string cmd = "xdg-open ";
    cmd += url;
    int ret = system(cmd.c_str());
    if (ret != 0)
    {
        std::cerr << "Failed to execute command: " << cmd << std::endl;
    }
}
#endif // _WIN32

std::string buildCommand(const std::string& exe, const std::string& args) {
#ifdef _WIN32
    return "cmd /c \"chcp 936 > nul && \"" + exe + "\" " + osgDB::convertStringFromUTF8toCurrentCodePage(args) + "\"";
#else
    return exe + " " + args + " 2>&1";
#endif
}

// 自定义风格
void setupCustomStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowBorderSize = 1.5f;
    style.FrameBorderSize = 1.0f;
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(6, 6);
    style.ScrollbarRounding = 6.0f;

    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.14f, 0.17f, 0.95f);

    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.27f, 0.32f, 0.38f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.27f, 0.32f, 1.00f);

    colors[ImGuiCol_TitleBg] = ImVec4(0.16f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.26f, 0.32f, 1.00f);

    colors[ImGuiCol_Header] = ImVec4(0.23f, 0.28f, 0.34f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.36f, 0.42f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.40f, 0.46f, 1.00f);

    colors[ImGuiCol_Button] = ImVec4(0.26f, 0.31f, 0.38f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.42f, 0.50f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.27f, 0.34f, 1.00f);

    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.20f, 0.27f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.40f, 0.45f, 0.52f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.26f, 0.31f, 0.38f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.20f, 0.27f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.25f, 0.32f, 1.00f);

    colors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.42f, 0.50f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.47f, 0.55f, 1.00f);

    colors[ImGuiCol_CheckMark] = ImVec4(0.45f, 0.55f, 0.65f, 1.00f);
}

static void showHelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", desc);
}

void runExternalTool(const std::string& exePath, const std::string& args,const int index) {
    isConverting = true;
    conversionLogs[index].clear();
    conversionLogs[index] += "当前执行命令:" + exePath + args + "\n";

    std::thread([exePath, args,index]() {
        std::ostringstream fullOutput;

#ifdef _WIN32
        HANDLE hReadPipe, hWritePipe;
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);

        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;

        PROCESS_INFORMATION pi = {};
#ifndef NDEBUG
        std::string cmd = "cmd /c \"chcp 936 > nul && \"" + exePath + "\" " + args + "\"";
#else
        std::string cmd = "cmd /c \"chcp 936 > nul && \"" + exePath + "\" " + osgDB::convertStringFromUTF8toCurrentCodePage(args) + "\"";//解决中文乱码问题
#endif // !NDEBUG


        BOOL success = CreateProcessA(
            NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi
        );

        if (success) {
            pis.push_back(pi);
            CloseHandle(hWritePipe);

            char buffer[512];
            DWORD bytesRead;
            while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                {
                    std::lock_guard<std::mutex> lock(logMutex);
                    conversionLogs[index] += buffer;
                }
                scrollToBottom = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 防止UI卡顿
            }

            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        else {
            fullOutput << "无法启动转换程序。\n";
        }

        CloseHandle(hReadPipe);
#else
        FILE* pipe = popen(buildCommand(exePath, args).c_str(), "r");
        if (!pipe) {
            fullOutput << "无法启动转换程序。\n";
        }
        else {
            char buffer[512];
            while (fgets(buffer, sizeof(buffer), pipe)) {
                {
                    std::lock_guard<std::mutex> lock(logMutex);
                    conversionLogs[index] += buffer;
                }
                scrollToBottom = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            pclose(pipe);
        }
#endif

        conversionLogs[index] += fullOutput.str();
        isConverting = false;
        }).detach();
}

void createLog(const int index) {
    ImGui::Text("转换日志：");
    // 在同一行插入复制按钮，并将其推到最右边
    ImGui::SameLine();
    float fullWidth = ImGui::GetContentRegionAvail().x;
    float buttonWidth = ImGui::CalcTextSize("复制日志").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + fullWidth - buttonWidth);

    if (ImGui::Button("复制日志")) {
        ImGui::LogToClipboard();
        ImGui::LogText("%s", conversionLogs[index].c_str());
        ImGui::LogFinish();
    }
    ImGui::BeginChild("ConversionLog", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(conversionLogs[index].c_str());
    if (scrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottom = false;
    }
    ImGui::EndChild();
}

void createModel23dtilesToolTab(bool enable) {
    ImGui::BeginDisabled(!enable);
    if (ImGui::BeginTabItem("model23dtiles"))
    {

        //ImGui::TextWrapped("将3D模型转换为3DTiles格式，支持多种压缩及组织结构。");
        //ImGui::Separator();

        // 输入输出参数
        if (ImGui::CollapsingHeader("输入输出参数", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            ImGui::BeginChild("IOGroup", ImVec2(0, 130), true);
            {
                // ===== 输入文件 =====
                ImGui::Text("输入文件 (-i)");
                ImGui::InputText("##inputModel", modelParams.inputModel, IM_ARRAYSIZE(modelParams.inputModel));
                ImGui::SameLine();
                if (ImGui::Button("选择文件")) {
                    ImGuiFileDialog::Instance()->OpenDialog(
                        "ChooseInputFile",
                        "选择输入文件",
                        "3D 模型文件 (*.fbx *.obj *.3ds *.dxf *.ive *.osg *.osgb){.fbx,.obj,.3ds,.dxf,.ive,.osg,.osgb},所有文件{.*}" // 支持的文件类型
                    );
                }


                // ===== 输出文件夹 =====
                ImGui::Text("输出文件夹 (-o)");
                ImGui::InputText("##outputDir", modelParams.outputDir, IM_ARRAYSIZE(modelParams.outputDir));
                ImGui::SameLine();
                if (ImGui::Button("选择文件夹")) {
                    ImGuiFileDialog::Instance()->OpenDialog(
                        "ChooseOutputDir",
                        "选择输出文件夹",
                        nullptr // 设为 nullptr 表示只选择文件夹
                    );
                }

                // ===== 显示文件选择窗口并处理结果 =====
                if (ImGuiFileDialog::Instance()->Display("ChooseInputFile", ImGuiWindowFlags_None, ImVec2(400, 300), ImVec2(800, 600))) {
                    if (ImGuiFileDialog::Instance()->IsOk()) {
                        std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
                        strncpy(modelParams.inputModel, filePath.c_str(), IM_ARRAYSIZE(modelParams.inputModel));
                    }
                    ImGuiFileDialog::Instance()->Close();
                }

                // ===== 显示文件夹选择窗口并处理结果 =====
                if (ImGuiFileDialog::Instance()->Display("ChooseOutputDir", ImGuiWindowFlags_None, ImVec2(400, 300), ImVec2(800, 600))) {
                    if (ImGuiFileDialog::Instance()->IsOk()) {
                        std::string dirPath = ImGuiFileDialog::Instance()->GetCurrentPath();
                        strncpy(modelParams.outputDir, dirPath.c_str(), IM_ARRAYSIZE(modelParams.outputDir));
                    }
                    ImGuiFileDialog::Instance()->Close();
                }

            }
            ImGui::EndChild();
            ImGui::Unindent();
        }


        // 坐标参数
        ImGui::Separator();
        if (ImGui::CollapsingHeader("坐标参数", ImGuiTreeNodeFlags_CollapsingHeader)) {
            ImGui::Indent();
            ImGui::BeginChild("CoordGroup", ImVec2(0, 280), true);
            {
                if (ImGui::Checkbox("使用经纬度/高度参数", &modelParams.useLatLngAlt)) {}

                ImGui::BeginDisabled(!modelParams.useLatLngAlt);
                ImGui::Text("纬度 (-lat)");
                double minLat = -90.0, maxLat = 90.0;
                double minLng = -180.0, maxLng = 180.0;
                double minAlt = -100000000.0, maxAlt = 100000000.0;
                ImGui::DragScalar("##lat", ImGuiDataType_Double, &modelParams.lat, 0.00000001, &minLat, &maxLat, "%.8f");
                ImGui::SameLine(); showHelpMarker("模型参考的纬度，默认30.0。");

                ImGui::Text("经度 (-lng)");
                ImGui::DragScalar("##lng", ImGuiDataType_Double, &modelParams.lng, 0.00000001, &minLng, &maxLng, "%.8f");
                ImGui::SameLine(); showHelpMarker("模型参考的经度，默认116.0。");

                ImGui::Text("高度 (-alt)");
                ImGui::DragScalar("##alt", ImGuiDataType_Double, &modelParams.alt, 0.001f, &minAlt, &maxAlt, "%.4f");
                ImGui::SameLine(); showHelpMarker("模型参考的海拔高度，默认300。");
                ImGui::EndDisabled();

                ImGui::BeginDisabled(modelParams.useLatLngAlt);
                ImGui::Text("EPSG编码 (-epsg)");
                ImGui::InputText("##epsg", modelParams.epsgStr, IM_ARRAYSIZE(modelParams.epsgStr));
                ImGui::SameLine(); showHelpMarker("若模型顶点坐标为投影坐标系，指定EPSG编码。该参数与lat/lng/alt互斥。");
                ImGui::EndDisabled();
            }
            ImGui::EndChild();
            ImGui::Unindent();
        }

        // 变换参数
        ImGui::Separator();
        if (ImGui::CollapsingHeader("变换参数", ImGuiTreeNodeFlags_CollapsingHeader)) {
            ImGui::Indent();
            ImGui::BeginChild("TransformGroup", ImVec2(0, 440), true);
            {

                ImGui::Text("X方向偏移 (-tx)");
                ImGui::DragScalar("##tx", ImGuiDataType_Double, &modelParams.tx, 0.01);
                ImGui::SameLine(); showHelpMarker("模型原点在X方向的偏移，单位和模型单位一致，默认0.0。");

                ImGui::Text("Y方向偏移 (-ty)");
                ImGui::DragScalar("##ty", ImGuiDataType_Double, &modelParams.ty, 0.01);
                ImGui::SameLine(); showHelpMarker("模型原点在Y方向的偏移，单位和模型单位一致，默认0.0。");

                ImGui::Text("Z方向偏移 (-tz)");
                ImGui::DragScalar("##tz", ImGuiDataType_Double, &modelParams.tz, 0.01);
                ImGui::SameLine(); showHelpMarker("模型原点在Z方向的偏移，单位和模型单位一致，默认0.0。");

                double minVal = 0.0001, maxVal = 10000.0;
                ImGui::Text("X轴缩放 (-sx)");
                ImGui::DragScalar("##sx", ImGuiDataType_Double, &modelParams.sx, 0.001, &minVal, &maxVal);
                ImGui::SameLine(); showHelpMarker("X方向缩放（单位转换），默认1.0。");

                ImGui::Text("Y轴缩放 (-sy)");
                ImGui::DragScalar("##sy", ImGuiDataType_Double, &modelParams.sy, 0.001, &minVal, &maxVal);
                ImGui::SameLine(); showHelpMarker("Y方向缩放（单位转换），默认1.0。");

                ImGui::Text("Z轴缩放 (-sz)");
                ImGui::DragScalar("##sz", ImGuiDataType_Double, &modelParams.sz, 0.001f, &minVal, &maxVal);
                ImGui::SameLine(); showHelpMarker("Z方向缩放（单位转换），默认1.0。");

                ImGui::Text("向上轴 (-up)");
                ImGui::Combo("##upAxis", &modelParams.upAxis, modelParams.upAxisItems, IM_ARRAYSIZE(modelParams.upAxisItems));
                ImGui::SameLine(); showHelpMarker("模型向上方向轴，选项：X、Y、Z，默认Y。");
            }
            ImGui::EndChild();
            ImGui::Unindent();
        }

        // 组织结构参数
        ImGui::Separator();
        if (ImGui::CollapsingHeader("组织结构参数", ImGuiTreeNodeFlags_CollapsingHeader)) {
            ImGui::Indent();
            ImGui::BeginChild("StructureGroup", ImVec2(0, 70), true);
            {
                ImGui::Text("树结构类型 (-t)");
                ImGui::Combo("##treeType", &modelParams.treeType, modelParams.treeTypes, IM_ARRAYSIZE(modelParams.treeTypes));
                ImGui::SameLine(); showHelpMarker("3dtiles组织结构，可选：kd（KD树）、quad（四叉树）、oc（八叉树），默认quad。");
            }
            ImGui::EndChild();
            ImGui::Unindent();
        }

        // 压缩与简化参数
        ImGui::Separator();
        if (ImGui::CollapsingHeader("压缩与简化参数", ImGuiTreeNodeFlags_CollapsingHeader)) {
            ImGui::Indent();
            ImGui::BeginChild("CompressionGroup", ImVec2(0, 240), true);
            {
                double minVal = 0.01, maxVal = 1.0;
                ImGui::Text("中间节点简化比例 (-r)");
                ImGui::DragScalar("##r", ImGuiDataType_Double, &modelParams.r, 0.01, &minVal, &maxVal);
                ImGui::SameLine(); showHelpMarker("3dtiles中间节点简化比例，默认0.5。");

                ImGui::Text("纹理压缩格式 (-tf)");
                ImGui::Combo("##tf", &modelParams.tf, modelParams.texFormats, IM_ARRAYSIZE(modelParams.texFormats));
                ImGui::SameLine(); showHelpMarker("纹理压缩格式，默认ktx2。");

                ImGui::Text("顶点压缩格式 (-vf)");
                ImGui::Combo("##vf", &modelParams.vf, modelParams.vertFormats, IM_ARRAYSIZE(modelParams.vertFormats));
                ImGui::SameLine(); showHelpMarker("顶点压缩格式，无默认值。支持四种压缩格式：draco、meshopt、quantize（顶点量化）、quantize_meshopt(顶点量化+meshopt)");

                ImGui::BeginDisabled(modelParams.vf == 0 || modelParams.vf == 2);
                ImGui::Text("压缩级别 (-cl)");
                ImGui::Combo("##cl", &modelParams.cl, modelParams.clLevels, IM_ARRAYSIZE(modelParams.clLevels));
                ImGui::SameLine(); showHelpMarker("仅当压缩格式为：draco、quantize和quantize_meshopt时生效，默认medium。压缩级别越高，压缩后的瓦片体积越小，但是模型精度损失越大。");
                ImGui::EndDisabled();
            }
            ImGui::EndChild();
            ImGui::Unindent();
        }

        // 性能限制参数
        ImGui::Separator();
        if (ImGui::CollapsingHeader("性能限制参数", ImGuiTreeNodeFlags_CollapsingHeader)) {
            ImGui::Indent();
            ImGui::BeginChild("PerformanceGroup", ImVec2(0, 360), true);
            {

                ImGui::Text("最大三角面数 (-tri)");
                ImGui::InputInt("##tri", &modelParams.tri);
                modelParams.tri = std::max(10000, modelParams.tri);
                modelParams.tri = std::min(2000000, modelParams.tri);
                ImGui::SameLine(); showHelpMarker("3dtiles单个瓦片最大三角面数，默认20万。该值越大，最终生成单个瓦片也体积越大，瓦片总数量会减少，单个瓦片渲染耗费性能也越大。");

                ImGui::Text("最大drawcall数量 (-dc)");
                ImGui::InputInt("##dc", &modelParams.dc);
                modelParams.dc = std::max(5, modelParams.dc);
                modelParams.dc = std::min(500, modelParams.dc);
                ImGui::SameLine(); showHelpMarker("3dtiles瓦片最大drawcall数量，默认20。该值越大，最终生成单个瓦片也体积越大，瓦片总数量会减少，单个瓦片渲染耗费性能也越大。");

                ImGui::Text("单个纹理最大宽度 (-tw)");
                ImGui::InputInt("##tw", &modelParams.tw);
                modelParams.tw = std::max(2, modelParams.tw);
                modelParams.tw = std::min(4096, modelParams.tw);
                ImGui::SameLine(); showHelpMarker("单个纹理最大宽度，需为2的幂，默认256。");

                ImGui::Text("单个纹理最大高度 (-th)");
                ImGui::InputInt("##th", &modelParams.th);
                modelParams.th = std::max(2, modelParams.th);
                modelParams.th = std::min(4096, modelParams.th);
                ImGui::SameLine(); showHelpMarker("单个纹理最大高度，需为2的幂，默认256。");

                ImGui::Text("纹理图集最大宽度 (-aw)");
                ImGui::InputInt("##aw", &modelParams.aw);
                modelParams.aw = std::max(2, modelParams.aw);
                modelParams.aw = std::min(8192, modelParams.aw);
                ImGui::SameLine(); showHelpMarker("纹理图集最大宽度，需为2的幂，且大于单个纹理宽度，默认2048。");

                ImGui::Text("纹理图集最大高度 (-ah)");
                ImGui::InputInt("##ah", &modelParams.ah);
                modelParams.ah = std::max(2, modelParams.ah);
                modelParams.ah = std::min(8192, modelParams.ah);
                ImGui::SameLine(); showHelpMarker("纹理图集最大高度，需为2的幂，且大于单个纹理高度，默认2048。");
            }
            ImGui::EndChild();
            ImGui::Unindent();
        }

        // 其他参数
        ImGui::Separator();
        if (ImGui::CollapsingHeader("其他参数", ImGuiTreeNodeFlags_CollapsingHeader)) {
            ImGui::Indent();
            ImGui::BeginChild("OtherGroup", ImVec2(0, 200), true);
            {

                ImGui::Checkbox("不应用变换矩阵 (-nft)", &modelParams.nft);
                ImGui::SameLine(); showHelpMarker("默认对顶点应用变换矩阵以提升性能，启用此选项则不应用。对顶点应用变换矩阵后可能会影响模型精度，如果未启用该选项时发现模型精度受损了，则应启用该选项。");

                ImGui::Checkbox("重新计算法线 (-nrm)", &modelParams.nrm);
                ImGui::SameLine(); showHelpMarker("重新计算模型法线。");

                ImGui::BeginDisabled(!modelParams.nrm);
                ImGui::Text("法线模式 (-nm)");
                ImGui::Combo("##nm", &modelParams.nm, modelParams.nmModes, IM_ARRAYSIZE(modelParams.nmModes));
                ImGui::SameLine(); showHelpMarker("法线模式，f表示面法线，v表示顶点法线，默认f。");
                ImGui::EndDisabled();

                ImGui::Checkbox("启用KHR_materials_unlit (-unlit)", &modelParams.unlit);
                ImGui::SameLine(); showHelpMarker("启用无光照材质扩展，适合烘焙模型。");

                ImGui::Checkbox("生成法线贴图和切线 (-gn)", &modelParams.gn);
                ImGui::SameLine(); showHelpMarker("生成法线贴图（Sobel算子）和切线，提升渲染效果（有限），但体积和时间增加。");
            }
            ImGui::EndChild();
            ImGui::Unindent();
        }

        ImGui::Dummy(ImVec2(0, 10)); // 底部空隙

        ImGui::BeginDisabled(isConverting);
        if (ImGui::Button("转换", ImVec2(150, 40))) {
#ifdef _WIN32
            currentToolName = "model23dtiles.exe";
#else
            currentToolName = "./model23dtiles";
#endif

            std::ostringstream args;
            args << " -sp"
                << " -i " << modelParams.inputModel
                << " -o " << modelParams.outputDir;

            if (modelParams.useLatLngAlt) {
                args << " -lat " << modelParams.lat
                    << " -lng " << modelParams.lng
                    << " -alt " << modelParams.alt;
            }
            else {
                args << " -epsg " << modelParams.epsgStr;
            }

            // 简化重复字段添加
            auto addVec3 = [&](const std::string& prefix, double x, double y, double z) {
                args << " " << prefix << "x " << x
                    << " " << prefix << "y " << y
                    << " " << prefix << "z " << z;
                };

            addVec3("-t", modelParams.tx, modelParams.ty, modelParams.tz);
            addVec3("-s", modelParams.sx, modelParams.sy, modelParams.sz);

            args << " -t " << modelParams.treeTypes[modelParams.treeType]
                << " -r " << modelParams.r
                << " -tf " << modelParams.texFormats[modelParams.tf];

            if (modelParams.vf) {
                args << " -vf " << modelParams.vertFormats[modelParams.vf]
                    << " -cl " << modelParams.clLevels[modelParams.cl];
            }

            args << " -tri " << modelParams.tri
                << " -dc " << modelParams.dc
                << " -tw " << modelParams.tw
                << " -th " << modelParams.th
                << " -aw " << modelParams.aw
                << " -ah " << modelParams.ah;

            // 可选布尔参数
            if (modelParams.nft)   args << " -ntf";
            if (modelParams.gn)    args << " -gn";
            if (modelParams.unlit) args << " -unlit";
            if (modelParams.nrm) {
                args << " -nrm";
                args << " -nm " << (modelParams.nm == 0 ? "f" : "v");
            }

            runExternalTool(currentToolName, args.str(), 0);
        }
        ImGui::EndDisabled();

        createLog(0);

        ImGui::EndTabItem();
    }
    ImGui::EndDisabled();
}

void createB3dm2gltfToolTab(bool enable) {
    ImGui::BeginDisabled(!enable);
    if (ImGui::BeginTabItem("b3dm2gltf")) {
        static char inputB3dm[256] = R"(C:)";
        static char outputFile[256] = R"(C:)";
        if (ImGui::CollapsingHeader("输入输出参数", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            ImGui::BeginChild("b3dmIO", ImVec2(0, 130), true);
            {

                ImGui::Text("输入文件 (-i)");
                ImGui::InputText("##inputB3dm", inputB3dm, IM_ARRAYSIZE(inputB3dm));
                ImGui::SameLine();
                if (ImGui::Button("选择输入文件")) {
                    ImGuiFileDialog::Instance()->OpenDialog(
                        "ChooseInputFile",
                        "选择输入文件",
                        "3DTiles瓦片文件 (*.b3dm){.b3dm}" // 支持的文件类型
                    );
                }

                ImGui::Text("输出文件夹 (-o)");
                ImGui::InputText("##outputFileB3dm", outputFile, IM_ARRAYSIZE(outputFile));
                ImGui::SameLine();
                if (ImGui::Button("选择输出文件")) {
                    ImGuiFileDialog::Instance()->OpenDialog(
                        "ChooseOutputFile",
                        "选择输出文件",
                        "GLTF文件 (*.gltf *.glb){.gltf,.glb}" // 支持的文件类型
                    );
                }


                // ===== 显示文件选择窗口并处理结果 =====
                if (ImGuiFileDialog::Instance()->Display("ChooseInputFile", ImGuiWindowFlags_None, ImVec2(400, 300), ImVec2(800, 600))) {
                    if (ImGuiFileDialog::Instance()->IsOk()) {
                        std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
                        strncpy(inputB3dm, filePath.c_str(), IM_ARRAYSIZE(inputB3dm));
                    }
                    ImGuiFileDialog::Instance()->Close();
                }

                // ===== 显示文件夹选择窗口并处理结果 =====
                if (ImGuiFileDialog::Instance()->Display("ChooseOutputFile", ImGuiWindowFlags_None, ImVec2(400, 300), ImVec2(800, 600))) {
                    if (ImGuiFileDialog::Instance()->IsOk()) {
                        std::string dirPath = ImGuiFileDialog::Instance()->GetFilePathName();
                        strncpy(outputFile, dirPath.c_str(), IM_ARRAYSIZE(outputFile));
                    }
                    ImGuiFileDialog::Instance()->Close();
                }
            }
            ImGui::EndChild();
            ImGui::Unindent();
        }
        ImGui::Dummy(ImVec2(0, 10));
        if (ImGui::Button("转换", ImVec2(150, 40))) {
#ifdef _WIN32
            currentToolName = "b3dm2gltf.exe";
#else
            currentToolName = "./b3dm2gltf";
#endif
            std::ostringstream args;
            args << " -i " << inputB3dm
                << " -o " << outputFile;
            runExternalTool(currentToolName, args.str(), 1);
        }

        createLog(1);
        ImGui::EndTabItem();
    }
    ImGui::EndDisabled();
}

void createSimplifierToolTab(bool enable) {
    ImGui::BeginDisabled(!enable);
    if (ImGui::BeginTabItem("simplifier")) {

        ImGui::Text("输入输出参数");
        ImGui::Indent();
        ImGui::BeginChild("simplifierIO", ImVec2(0, 130), true);
        {
            static char inputFile[256] = "D:\\input.obj";
            static char outputFile[256] = "D:\\output.obj";

            ImGui::Text("输入文件 (-i)");
            ImGui::InputText("##inputSimplifier", inputFile, IM_ARRAYSIZE(inputFile));
            ImGui::SameLine(); showHelpMarker("待简化模型路径。");

            ImGui::Text("输出文件 (-o)");
            ImGui::InputText("##outputSimplifier", outputFile, IM_ARRAYSIZE(outputFile));
            ImGui::SameLine(); showHelpMarker("简化后模型保存路径。");
        }
        ImGui::EndChild();
        ImGui::Unindent();

        ImGui::Separator();
        ImGui::Text("简化参数");
        ImGui::Indent();
        ImGui::BeginChild("simplifierParams", ImVec2(0, 130), true);
        {
            static int targetTriangles = 10000;
            ImGui::Text("目标三角面数 (-t)");
            ImGui::InputInt("##targetTriangles", &targetTriangles);
            ImGui::SameLine(); showHelpMarker("简化后模型目标三角面数。");
        }
        ImGui::EndChild();
        ImGui::Unindent();

        ImGui::Dummy(ImVec2(0, 10));
        if (ImGui::Button("执行简化", ImVec2(150, 40))) {
            // TODO
        }

        ImGui::EndTabItem();
    }
    ImGui::EndDisabled();
}

void createTexturepackerToolTab(bool enable) {
    ImGui::BeginDisabled(!enable);
    if (ImGui::BeginTabItem("texturepacker")) {

        ImGui::Text("输入输出参数");
        ImGui::Indent();
        ImGui::BeginChild("texturepackerIO", ImVec2(0, 130), true);
        {
            static char inputDir[256] = R"(C:)";
            static char outputFile[256] = R"(C:)";

            ImGui::Text("输入纹理目录 (-i)");
            ImGui::InputText("##inputDir", inputDir, IM_ARRAYSIZE(inputDir));
            ImGui::SameLine(); showHelpMarker("纹理图片所在文件夹。");

            ImGui::Text("输出文件 (-o)");
            ImGui::InputText("##outputTexture", outputFile, IM_ARRAYSIZE(outputFile));
            ImGui::SameLine(); showHelpMarker("生成的纹理图集路径。");
        }
        ImGui::EndChild();
        ImGui::Unindent();

        ImGui::Separator();
        ImGui::Text("打包参数");
        ImGui::Indent();
        ImGui::BeginChild("texturepackerParams", ImVec2(0, 130), true);
        {
            static int maxAtlasWidth = 2048;
            static int maxAtlasHeight = 2048;
            ImGui::Text("最大图集宽度 (-aw)");
            ImGui::InputInt("##maxAtlasWidth", &maxAtlasWidth);
            ImGui::SameLine(); showHelpMarker("纹理图集最大宽度，需为2的幂。");

            ImGui::Text("最大图集高度 (-ah)");
            ImGui::InputInt("##maxAtlasHeight", &maxAtlasHeight);
            ImGui::SameLine(); showHelpMarker("纹理图集最大高度，需为2的幂。");
        }
        ImGui::EndChild();
        ImGui::Unindent();

        ImGui::Dummy(ImVec2(0, 10));
        if (ImGui::Button("执行打包", ImVec2(150, 40))) {
            // TODO
        }

        ImGui::EndTabItem();
    }
    ImGui::EndDisabled();
}

void createTabs() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("工具箱", nullptr, window_flags);

    if (ImGui::BeginTabBar("ToolsTabBar", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_Reorderable)) {
        createModel23dtilesToolTab(true);
        createB3dm2gltfToolTab(true);
        //createSimplifierToolTab(true);
        //createTexturepackerToolTab(true);
        ImGui::EndTabBar();
    }

    ImGui::End();
}

static void glfw_error_callback(int error, const char* description)
{
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void drawFooter() {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    const float footer_height = ImGui::GetTextLineHeightWithSpacing() + 10.0f;
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - footer_height));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, footer_height));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.15f, 0.2f, 1.0f)); // 背景色
    ImGui::Begin("##FooterLinks", nullptr, window_flags);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::Text("源码地址：");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // GitHub 链接
    const char* github_url = "https://github.com/newpeople123/osgGISPlugins";
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.5f, 1.0f, 1.0f)); // 链接颜色
    ImGui::TextUnformatted("GitHub");
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); // 鼠标变手型
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            openURL(github_url);
        }
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::Text("|");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // Gitee 链接
    const char* gitee_url = "https://gitee.com/wtyhz/osg-gis-plugins";
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.5f, 1.0f, 1.0f)); // 链接颜色
    ImGui::TextUnformatted("Gitee");
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); // 鼠标变手型
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            openURL(gitee_url);
        }
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::Text("，欢迎交流与 Star 支持！");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::Text("邮箱地址：");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    const char* qq_url = "https://mail.qq.com/";
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.5f, 1.0f, 1.0f)); // 链接颜色
    ImGui::TextUnformatted("947643058@qq.com");
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); // 鼠标变手型
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            openURL(qq_url);
        }
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();

    ImGui::End();
    ImGui::PopStyleColor();
}

int main() {
#ifdef _WIN32
    // 隐藏控制台窗口（仅适用于 Windows 平台）
    HWND hWnd = GetConsoleWindow();
    ShowWindow(hWnd, SW_HIDE);
    SetConsoleCtrlHandler(consoleHandler, TRUE);
#endif

    if (!glfwInit()) {
        std::cerr << "无法初始化 GLFW\n";
        return -1;
    }

    // 设置 OpenGL 版本
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1000, 900, VERSION, nullptr, nullptr);
#ifdef _WIN32
    glfwSetWindowCloseCallback(window, onWindowClose);
#endif
    if (window == nullptr) {
        std::cerr << "窗口创建失败\n";
        glfwTerminate();
        return -1;
    }
    GLFWimage images[1];
    images[0].pixels = stbi_load("favicon.png", &images[0].width, &images[0].height, 0, 4); //rgba channels
    glfwSetWindowIcon(window, 1, images);
    stbi_image_free(images[0].pixels);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // 开启垂直同步

    // 初始化 ImGui 上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // 设置风格
    setupCustomStyle();

    ImGuiIO& io = ImGui::GetIO();

    // 加载支持中文的字体
    io.Fonts->AddFontFromFileTTF(R"(msyh.ttc)", 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());

    ImGui::StyleColorsLight();

    // 初始化 ImGui 平台/渲染后端
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // 主循环
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        createTabs();
        drawFooter();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // 清理
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
