#include "http_server.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>
#include <shellapi.h>

int main(int argc, char* argv[]) {
    // Set console title
    SetConsoleTitleA("PASS Unified Engineering Workbench Launcher");

    std::cout << "========================================================\n";
    std::cout << "       PASS UNIFIED ENGINEERING WORKBENCH v1.0          \n";
    std::cout << "       R&D Internship 2026-2027 Assessment Prototype   \n";
    std::cout << "========================================================\n\n";
    std::cout << "[*] Starting high-performance C++ backend server...\n";

    // Locate frontend relative to executable
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir(exePath);
    auto lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        exeDir = exeDir.substr(0, lastSlash);
    }

    std::string frontendRoot = exeDir + "\\src\\frontend";
    std::ifstream check(frontendRoot + "\\index.html");
    if (!check.is_open()) {
        frontendRoot = "src/frontend";
    }

    int port = 8080;
    Workbench::HttpServer server(port, frontendRoot);
    if (!server.start()) {
        std::cerr << "[!] Error: Failed to bind to local port. Please check firewall or port occupancy.\n";
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::string url = "http://127.0.0.1:" + std::to_string(server.getPort());
    std::cout << "[+] Server running at: " << url << "\n";
    std::cout << "[*] Opening default web browser in 1 second...\n";

    Sleep(500);
    // Launch default web browser
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);

    std::cout << "\n========================================================\n";
    std::cout << "  Workbench UI launched in your default browser!        \n";
    std::cout << "  Views available: Geometry, Thermal, Multicore, Hardware\n";
    std::cout << "  No terminal commands, admin rights or internet needed.\n";
    std::cout << "  Press [ENTER] in this window at any time to exit.     \n";
    std::cout << "========================================================\n\n";

    std::cin.get();

    std::cout << "[*] Shutting down server gracefully...\n";
    server.stop();
    return 0;
}
