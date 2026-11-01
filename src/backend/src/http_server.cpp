#include "http_server.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

namespace Workbench {

static std::string getMimeType(const std::string& path) {
    if (path.rfind(".html") != std::string::npos) return "text/html; charset=utf-8";
    if (path.rfind(".css") != std::string::npos) return "text/css; charset=utf-8";
    if (path.rfind(".js") != std::string::npos) return "application/javascript; charset=utf-8";
    if (path.rfind(".json") != std::string::npos) return "application/json; charset=utf-8";
    if (path.rfind(".csv") != std::string::npos) return "text/csv; charset=utf-8";
    if (path.rfind(".stl") != std::string::npos) return "text/plain; charset=utf-8";
    if (path.rfind(".png") != std::string::npos) return "image/png";
    if (path.rfind(".svg") != std::string::npos) return "image/svg+xml";
    return "application/octet-stream";
}

static std::map<std::string, std::string> parseQuery(const std::string& qs) {
    std::map<std::string, std::string> params;
    std::istringstream stream(qs);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        auto eq = pair.find('=');
        if (eq != std::string::npos) {
            params[pair.substr(0, eq)] = pair.substr(eq + 1);
        } else if (!pair.empty()) {
            params[pair] = "";
        }
    }
    return params;
}

std::string HttpResponse::serialize() const {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    ss << "Content-Type: " << contentType << "\r\n";
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Access-Control-Allow-Origin: *\r\n";
    ss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    ss << "Access-Control-Allow-Headers: Content-Type\r\n";
    ss << "Connection: close\r\n";
    for (const auto& h : headers) {
        ss << h.first << ": " << h.second << "\r\n";
    }
    ss << "\r\n";
    ss << body;
    return ss.str();
}

HttpServer::HttpServer(int port, const std::string& frontendRoot)
    : port_(port), frontendRoot_(frontendRoot) {
    deviceManager_ = std::make_unique<EmbeddedDeviceManager>();
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    if (running_) return true;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[HttpServer] WSAStartup failed.\n";
        return false;
    }

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        std::cerr << "[HttpServer] socket creation failed.\n";
        WSACleanup();
        return false;
    }

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port_);

    if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        // Try next port if port_ is occupied
        for (int p = port_ + 1; p < port_ + 10; ++p) {
            addr.sin_port = htons(p);
            if (bind(s, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR) {
                port_ = p;
                break;
            }
        }
    }

    if (listen(s, 16) == SOCKET_ERROR) {
        std::cerr << "[HttpServer] listen failed.\n";
        closesocket(s);
        WSACleanup();
        return false;
    }

    listenSocket_ = (uintptr_t)s;
    running_ = true;

    serverThread_ = std::thread(&HttpServer::serverLoop, this);
    std::cout << "[HttpServer] Listening on http://127.0.0.1:" << port_ << "\n";
    return true;
}

void HttpServer::stop() {
    if (!running_) return;
    running_ = false;

    if (listenSocket_ != 0) {
        closesocket((SOCKET)listenSocket_);
        listenSocket_ = 0;
    }

    if (serverThread_.joinable()) {
        serverThread_.join();
    }

    WSACleanup();
    std::cout << "[HttpServer] Stopped.\n";
}

void HttpServer::serverLoop() {
    while (running_) {
        sockaddr_in clientAddr;
        int clientLen = sizeof(clientAddr);
        SOCKET client = accept((SOCKET)listenSocket_, (sockaddr*)&clientAddr, &clientLen);
        if (client == INVALID_SOCKET) {
            if (!running_) break;
            continue;
        }

        // Handle each client in a lightweight detached thread or immediately
        std::thread([this, client]() {
            handleClient((uintptr_t)client);
        }).detach();
    }
}

void HttpServer::handleClient(uintptr_t clientSocket) {
    SOCKET s = (SOCKET)clientSocket;
    char buffer[16384];
    std::string rawRequest;

    while (true) {
        int bytesRead = recv(s, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) break;
        buffer[bytesRead] = '\0';
        rawRequest.append(buffer, bytesRead);

        // Check if header complete
        auto headerEnd = rawRequest.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            // Find content length if present
            auto clPos = rawRequest.find("Content-Length: ");
            if (clPos == std::string::npos) clPos = rawRequest.find("content-length: ");
            if (clPos != std::string::npos) {
                size_t cl = std::stoul(rawRequest.substr(clPos + 16, rawRequest.find("\r\n", clPos) - (clPos + 16)));
                size_t bodyGot = rawRequest.size() - (headerEnd + 4);
                if (bodyGot >= cl) break;
            } else {
                break;
            }
        }
    }

    if (rawRequest.empty()) {
        closesocket(s);
        return;
    }

    // Parse request line
    std::istringstream stream(rawRequest);
    std::string reqLine;
    std::getline(stream, reqLine);
    if (!reqLine.empty() && reqLine.back() == '\r') reqLine.pop_back();

    std::istringstream rlStream(reqLine);
    HttpRequest req;
    rlStream >> req.method >> req.path;

    if (req.method == "OPTIONS") {
        HttpResponse resp;
        resp.statusCode = 204;
        resp.statusText = "No Content";
        std::string raw = resp.serialize();
        send(s, raw.c_str(), (int)raw.size(), 0);
        closesocket(s);
        return;
    }

    // Split path and queryString
    auto qPos = req.path.find('?');
    if (qPos != std::string::npos) {
        req.queryString = req.path.substr(qPos + 1);
        req.path = req.path.substr(0, qPos);
    }

    auto headerEnd = rawRequest.find("\r\n\r\n");
    if (headerEnd != std::string::npos) {
        req.body = rawRequest.substr(headerEnd + 4);
    }

    HttpResponse resp = dispatch(req);
    std::string rawResp = resp.serialize();
    send(s, rawResp.c_str(), (int)rawResp.size(), 0);
    closesocket(s);
}

HttpResponse HttpServer::dispatch(const HttpRequest& req) {
    HttpResponse resp;

    // API Routing
    if (req.path == "/api/status") {
        resp.body = "{\n"
            "  \"status\": \"ONLINE\",\n"
            "  \"service\": \"Unified Engineering Workbench Core Server\",\n"
            "  \"cxxStandard\": \"C++17 / C++20 (GCC 14.1.0)\",\n"
            "  \"system\": {\n"
            "    \"os\": \"Windows 11 x64\",\n"
            "    \"logicalCores\": 12,\n"
            "    \"physicalCores\": 6,\n"
            "    \"ramGB\": 7.35\n"
            "  }\n"
            "}";
        return resp;
    }

    if (req.path == "/api/stl/parse" && req.method == "POST") {
        auto mesh = STLParser::parseAsciiFromString(req.body);
        resp.body = mesh.toJson();
        return resp;
    }

    if (req.path == "/api/ansys/parse" && req.method == "POST") {
        auto ansys = ANSYSCSVParser::parseCSVFromString(req.body);
        resp.body = ansys.toJson();
        return resp;
    }

    if (req.path == "/api/fea/solve") {
        auto params = parseQuery(req.queryString);
        FEARodParams p;
        if (params.count("L")) p.L = std::stod(params["L"]);
        if (params.count("A")) p.A = std::stod(params["A"]);
        if (params.count("k")) p.k = std::stod(params["k"]);
        if (params.count("q")) p.q = std::stod(params["q"]);
        if (params.count("T0")) p.T0 = std::stod(params["T0"]);
        if (params.count("TL")) p.TL = std::stod(params["TL"]);
        if (params.count("N")) p.N = std::stoi(params["N"]);

        auto sol = FEARodSolver::solve(p);
        resp.body = sol.toJson();
        return resp;
    }

    if (req.path == "/api/fea/convergence") {
        auto study = FEARodSolver::runConvergenceStudy();
        resp.body = study.toJson();
        return resp;
    }

    if (req.path == "/api/multicore/benchmark") {
        BatchRunConfig cfg;
        auto params = parseQuery(req.queryString);
        if (params.count("cases")) cfg.caseCount = std::stoi(params["cases"]);
        if (params.count("meshN")) cfg.meshN = std::stoi(params["meshN"]);
        if (params.count("seed")) cfg.seed = static_cast<unsigned int>(std::stoul(params["seed"]));

        auto report = MulticoreBatchEngine::runBenchmark(cfg);
        resp.body = report.toJson();
        return resp;
    }

    if (req.path == "/api/devices") {
        resp.body = deviceManager_->getAllDevicesStatusJson();
        return resp;
    }

    if (req.path == "/api/devices/connect" && req.method == "POST") {
        auto params = parseQuery(req.queryString);
        std::string id = params.count("id") ? params["id"] : "mcu-stm32h7";
        auto dev = deviceManager_->getDevice(id);
        if (dev) {
            bool ok = dev->connect();
            resp.body = std::string("{\"success\":") + (ok ? "true" : "false") +
                        ",\"state\":\"" + dev->getStateString() + "\"}";
        } else {
            resp.statusCode = 404;
            resp.body = "{\"error\":\"Device not found\"}";
        }
        return resp;
    }

    if (req.path == "/api/devices/disconnect" && req.method == "POST") {
        auto params = parseQuery(req.queryString);
        std::string id = params.count("id") ? params["id"] : "mcu-stm32h7";
        auto dev = deviceManager_->getDevice(id);
        if (dev) {
            dev->disconnect();
            resp.body = "{\"success\":true,\"state\":\"DISCONNECTED\"}";
        } else {
            resp.statusCode = 404;
            resp.body = "{\"error\":\"Device not found\"}";
        }
        return resp;
    }

    if (req.path == "/api/devices/fault" && req.method == "POST") {
        auto params = parseQuery(req.queryString);
        std::string id = params.count("id") ? params["id"] : "mcu-stm32h7";
        auto dev = deviceManager_->getDevice(id);
        if (dev) {
            dev->injectTimeoutFault(true);
            resp.body = "{\"success\":true,\"state\":\"ERROR_TIMEOUT\",\"message\":\"Timeout fault injected.\"}";
        } else {
            resp.statusCode = 404;
            resp.body = "{\"error\":\"Device not found\"}";
        }
        return resp;
    }

    if (req.path == "/api/devices/recover" && req.method == "POST") {
        auto params = parseQuery(req.queryString);
        std::string id = params.count("id") ? params["id"] : "mcu-stm32h7";
        auto dev = deviceManager_->getDevice(id);
        if (dev) {
            bool ok = dev->recoverFromTimeout();
            resp.body = std::string("{\"success\":") + (ok ? "true" : "false") +
                        ",\"state\":\"" + dev->getStateString() + "\",\"message\":\"Recovered successfully.\"}";
        } else {
            resp.statusCode = 404;
            resp.body = "{\"error\":\"Device not found\"}";
        }
        return resp;
    }

    if (req.path == "/api/backprop/study") {
        auto report = BackpropEngine::runFullStudy(0.1, 100);
        resp.body = report.toJson();
        return resp;
    }

    if (req.path == "/api/export/csv") {
        auto params = parseQuery(req.queryString);
        std::string type = params.count("type") ? params["type"] : "fea";
        resp.contentType = "text/csv";
        if (type == "fea") {
            auto sol = FEARodSolver::solve(FEARodParams());
            resp.body = sol.toCSV();
            resp.headers.push_back({"Content-Disposition", "attachment; filename=\"fea_rod_results.csv\""});
        } else if (type == "convergence") {
            auto study = FEARodSolver::runConvergenceStudy();
            resp.body = study.toConvergenceCSV();
            resp.headers.push_back({"Content-Disposition", "attachment; filename=\"fea_mesh_convergence.csv\""});
        } else if (type == "multicore") {
            auto report = MulticoreBatchEngine::runBenchmark();
            resp.body = report.toCSV();
            resp.headers.push_back({"Content-Disposition", "attachment; filename=\"multicore_benchmark.csv\""});
        } else if (type == "backprop") {
            auto report = BackpropEngine::runFullStudy();
            resp.body = report.toCSV();
            resp.headers.push_back({"Content-Disposition", "attachment; filename=\"backprop_100_steps.csv\""});
        }
        return resp;
    }

    // Static File Serving
    std::string relPath = req.path;
    if (relPath == "/" || relPath.empty()) {
        relPath = "/index.html";
    }

    // Clean up path
    std::string filePath = frontendRoot_ + relPath;
    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        resp.statusCode = 404;
        resp.statusText = "Not Found";
        resp.contentType = "text/plain";
        resp.body = "404 Not Found: " + relPath;
        return resp;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    resp.contentType = getMimeType(relPath);
    resp.body = content;
    return resp;
}

} // namespace Workbench
