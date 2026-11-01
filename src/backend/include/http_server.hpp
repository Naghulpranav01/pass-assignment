#pragma once

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include "stl_parser.hpp"
#include "ansys_csv_parser.hpp"
#include "fea_1d_solver.hpp"
#include "multicore_batch.hpp"
#include "embedded_adapters.hpp"
#include "backprop.hpp"

namespace Workbench {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string queryString;
    std::string body;
    std::string contentType;
};

struct HttpResponse {
    int statusCode{200};
    std::string statusText{"OK"};
    std::string contentType{"application/json"};
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;

    std::string serialize() const;
};

class HttpServer {
public:
    HttpServer(int port = 8080, const std::string& frontendRoot = "src/frontend");
    ~HttpServer();

    bool start();
    void stop();
    int getPort() const { return port_; }
    bool isRunning() const { return running_; }

private:
    void serverLoop();
    void handleClient(uintptr_t clientSocket);
    HttpResponse dispatch(const HttpRequest& req);

    int port_{8080};
    std::string frontendRoot_;
    std::atomic<bool> running_{false};
    std::thread serverThread_;
    uintptr_t listenSocket_{0};

    // Subsystems
    std::unique_ptr<EmbeddedDeviceManager> deviceManager_;
};

} // namespace Workbench
