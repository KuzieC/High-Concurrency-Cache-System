#ifndef HTTPGATEWAY_H
#define HTTPGATEWAY_H

#include <gflags/gflags.h>
#include <httplib.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include "kcache/consistent_hash.h"

DEFINE_int32(http_port, 9000, "HTTP port");
DEFINE_string(etcd_endpoints, "http://127.0.0.1:2379", "etcd address");
DEFINE_string(service_name, "kcache", "cache service name");

class HttpGateway {
public:
    HttpGateway(int port, const std::string &etcd_endpoints, const std::string &service_name);
    ~HttpGateway();
    void StartService();    
private:
    void SetupRoute();
    void StartDiscovery();
    auto GetCacheNode(std::string &key);
    void Get(const httplib::Request &req, httplib::Response &res);
    void Set(const httplib::Request &req, httplib::Response &res);
    void Del(const httplib::Request &req, httplib::Response &res);
    int port_;
    std::string etcd_endpoints_;
    std::string service_name_;
    httplib::Server http_server_;
    std::shared_ptr<etcd::Client> etcd_client_;
    std::mutex mtx_;
    std::thread discovery_thread_;
    consistentHash consistent_hash_;
};

#endif // HTTPGATEWAY_H
