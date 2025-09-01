#include "include/httpgateway.h"
#include "cache.grpc.pb.h"
#include <nlohmann/json.hpp>
#include <etcd/Client.hpp>
#include <grpcpp/grpcpp.h>

HttpGateway::HttpGateway(int port, const std::string &etcd_endpoints, const std::string &service_name)
    : port_(port), etcd_endpoints_(etcd_endpoints), service_name_(service_name) {
        etcd_client_ = std::make_shared<etcd::Client>(etcd_endpoints_);
        SetupRoute();
        StartDiscovery();
    }

HttpGateway::~HttpGateway() {
    if (discovery_thread_.joinable()) {
        discovery_thread_.join();
    }
    http_server_.stop();
}

void HttpGateway::StartService() {
    spdlog::info("Starting HTTP Gateway on port {}", port_);
    http_server_.listen("0.0.0.0", port_);
}

void HttpGateway::SetupRoute() {
    http_server_.Get(R"(/([^/]+)/([^/]+))",
        [this](const httplib::Request& req, httplib::Response& res) { 
        HandleGet(req, res); });

    http_server_.Post(R"(/([^/]+)/([^/]+))",
        [this](const httplib::Request &req, httplib::Response &res) { 
        Set(req, res); });

    http_server_.Delete(R"(/([^/]+)/([^/]+))", 
        [this](const httplib::Request &req, httplib::Response &res) { 
        Del(req, res); 
    });
}

auto HttpGateway::GetCacheNode(std::string &key){
    std::lock_guard<std::mutex> lock(mtx_);
    std::string target = consistent_hash_.Get(key);
    if (target.empty()){
        spdlog::error("No available cache nodes");
        return "";
    }
    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    return channel;
}

void HttpGateway::Get(const httplib::Request &req, httplib::Response &res) {
    std::string group = req.matches[1];
    std::string key = req.matches[2];

    auto client = GetCacheNode(key);
    if(!client) {
        spdlog::error("Failed to get cache node for key: {}", key);
        res.status = 500;
        return;
    }
    cache::Request request;
    request.set_group(group);
    request.set_key(key);

    cache::GetResponse response;
    grpc::ClientContext context;
    grpc::Status status = client->Get(&context, request, &response);

    if (!status.ok()) {
        spdlog::error("gRPC call failed: {}", status.error_message());
        res.status = 404;
        return;
    }
    nlohmann::json json_resp = {{"key", key}, {"value", response.value()}, {"group", group}};
    res.set_content(json_resp.dump(), "application/json");
}

void HttpGateway::Set(const httplib::Request &req, httplib::Response &res) {
    std::string group = req.matches[1];
    std::string key = req.matches[2];

    auto client = GetCacheNode(key);
    if (!client) {
        spdlog::error("Failed to get cache node for key: {}", key);
        res.status = 500;
        return;
    }

    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req.body);
    } catch (const std::exception &e) {
        spdlog::error("Failed to parse JSON body: {}", e.what());
        res.status = 400;
        return;
    }

    std::string value = body.value("value", "");

    cache::Request request;
    request.set_group(group);
    request.set_key(key);
    request.set_value(value);

    cache::SetResponse response;
    grpc::ClientContext context;

    grpc::Status status = client->Set(&context, request, &response);

    if (!status.ok()) {
        spdlog::error("gRPC call failed: {}", status.error_message());
        res.status = 404;
        return;
    }
    nlohmann::json json_resp = {{"key", key}, {"value", value}, {"group", group}};
    res.set_content(json_resp.dump(), "application/json");
}

void HttpGateway::Del(const httplib::Request &req, httplib::Response &res) {
    std::string group = req.matches[1];
    std::string key = req.matches[2];

    auto client = GetCacheNode(key);
    if (!client) {
        spdlog::error("Failed to get cache node for key: {}", key);
        res.status = 500;
        return;
    }

    cache::Request request;
    request.set_group(group);
    request.set_key(key);

    cache::DelResponse response;
    grpc::ClientContext context;
    grpc::Status status = client->Del(&context, request, &response);

    if (!status.ok()) {
        spdlog::error("gRPC call failed: {}", status.error_message());
        res.status = 404;
        return;
    }
    nlohmann::json json_resp = {{"key", key}, {"group", group}};
    res.set_content(json_resp.dump(), "application/json");
}

void HttpGateway::StartDiscovery() {
    discovery_thread_ = std::thread([this]() {
        while (true) {
            std::string prefix = service_name_ + "/";
            try {
                auto respond = etcd_client_->ls(prefix).get();
                if (respond.is_ok()) {
                    std::lock_guard<std::mutex> lock(mtx_);
                    for (const auto& key : respond.keys()) {
                        if (key.rfind(prefix,0) == 0){
                            std::string addr = key.substr(prefix.length());
                            consistent_hash_.Add(addr);
                            spdlog::info("Added cache node: {}", addr);
                        }
                        else {
                            spdlog::warn("Key {} does not start with expected prefix {}", key, prefix);
                        }
                    }
                }
            } catch (const std::exception& e) {
                spdlog::error("Error occurred while fetching services: {}", e.what());
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });
}

int main(int argc, char* argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[http-gateway][%^%l%$] %v");

    try {
        HttpGateway gateway(FLAGS_http_port, FLAGS_etcd_endpoints, FLAGS_service_name);
        gateway.StartService();
    } catch (const std::exception &e) {
        spdlog::error("Failed to start HTTP Gateway: {}", e.what());
        return 1;
    }
    return 0;
}