#pragma once
#include <GLFW/glfw3.h>

namespace quantum {

class WindowFrame {
public:
    WindowFrame();
    ~WindowFrame();

    bool Initialize(int width, int height, const char* title);
    void Run();
    void Shutdown();

private:
    void RenderUI();
    void RenderBottomMenuBar();
    void SetupDockspace();
    void ProcessMarketDataUpdates();

    GLFWwindow* m_window = nullptr;
    const char* m_glsl_version = "#version 130";
};

} // namespace quantum
