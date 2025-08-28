#include "include/cacheserver.h"
#include "include/cachegroup.h"
#include <fmt/base.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <spdlog/spdlog.h>
#include <google/protobuf/any.pb.h>

#include <memory>

CacheServer::CacheServer(const std::string &service_addr, const std::string &service_name, const ServerOptions options)
    : service_addr_(service_addr), service_name_(service_name), options_(options) {
    etcd_registry_ = std::make_unique<etcdRegistry>(options_.etcd_endpoints[0]);
    spdlog::info("CacheServer created with service_addr: {}, service_name: {}", service_addr_, service_name_);
    if (etcd_registry_->Register(service_name_, service_addr_)) {
        spdlog::info("CacheServer registered with etcd: {}, {}", service_name_, service_addr_);
    } else {
        throw runtime_error("Failed to register service with etcd");
    }
}

void CacheServer::Start() {
    try {
        grpc::ServerBuilder builder;

        builder.AddListeningPort(service_addr_, grpc::InsecureServerCredentials());
        builder.RegisterService(this);
        server_ = builder.BuildAndStart();
        spdlog::info("CacheServer started at {}", service_addr_);
    } catch (const std::exception& e) {
        spdlog::error("Failed to start CacheServer: {}", e.what());
        throw;
    }
}

void CacheServer::Stop() {
    if (server_) {
        server_->Shutdown();
        spdlog::info("CacheServer at {} stopped", service_addr_);
    }
    if (etcd_registry_) {
        etcd_registry_->Unregister();
        spdlog::info("CacheServer unregistered from etcd: {}, {}", service_name_, service_addr_);
    }
}

grpc::Status CacheServer::Get(grpc::ServerContext* context, const cache::Request* request, cache::GetResponse* response) {
    auto group = CacheGroup<google::protobuf::Any>::GetCacheGroup(request->group());
    if(!group){
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Cache group not found");
    }
    auto val = group->Get(request->key());
    if(!val){
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Key not found");
    }
    response->mutable_value() = *val;
    return grpc::Status::OK;
}

grpc::Status CacheServer::Set(grpc::ServerContext* context, const cache::Request* request, cache::SetResponse* response) {
    auto* group = CacheGroup<google::protobuf::Any>::GetCacheGroup(request->group());
    if (!group) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Cache group not found");
    }
    
    group->Set(
        request->key(),
        request->value(),
        true  
    );
    
    response->set_value(true);
    return grpc::Status::OK;
}

grpc::Status CacheServer::Delete(grpc::ServerContext* context, const cache::Request* request,
                                cache::DeleteResponse* response) {
    auto* group = CacheGroup<google::protobuf::Any>::GetCacheGroup(request->group());
    if (!group) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Cache group not found");
    }
    
    group->Del(
        request->key(),
        true 
    );
    
    response->set_value(true);
    return grpc::Status::OK;
}

