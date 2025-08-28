#ifndef CACHESERVER_H
#define CACHESERVER_H
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>
#include <etcd/Client.hpp>

#include "cache.grpc.pb.h"
#include "cache.pb.h"
#include "include/registry.h"

struct ServerOptions {
    std::vector<std::string> etcd_endpoints;
    std::chrono::milliseconds dial_timeout;
    int max_msg_size;  // bytes
    bool tls;
    std::string cert_file;
    std::string key_file;

    ServerOptions()
        : etcd_endpoints({"http://127.0.0.1:2379"}),
          dial_timeout(std::chrono::seconds(5)),
          max_msg_size(4 << 20),  // 4MB
          tls(false) {}
};

class CacheServer final : public cache::Cache::Service{
public:
    CacheServer(const std::string &service_addr, const std::string &service_name, const ServerOptions options = ServerOptions());
    ~CacheServer();

    grpc::Status Get(grpc::ServerContext* context, const cache::Request* request,
                     cache::GetResponse* response) override;

    grpc::Status Set(grpc::ServerContext* context, const cache::Request* request,
                     cache::SetResponse* response) override;

    grpc::Status Delete(grpc::ServerContext* context, const cache::Request* request,
                        cache::DeleteResponse* response) override;
    void Start();
    void Stop();
private:
    std::string service_addr_;
    std::string service_name_;
    ServerOptions options_;
    std::unique_ptr<etcdRegistry> etcd_registry_;
    std::unique_ptr<grpc::Server> server_;
};



#endif // CACHESERVER_H