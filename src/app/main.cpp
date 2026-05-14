#include "WindowFrame.h"
#include <ixwebsocket/IXNetSystem.h>

int main()
{
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
