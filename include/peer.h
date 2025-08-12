#ifndef peer_h
#define peer_h

#include <fmt/core.h>
#include <google/protobuf/any.pb.h> 
#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <etcd/Client.hpp>
#include <etcd/Response.hpp>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>

#include "cache.grpc.pb.h"

class peer {
public:
    /**
     * @brief Constructs a Peer and establishes a gRPC connection.
     * @param name The address (ip:port) of the peer.
     */
    peer(const std::string& name): name_(name) {
        grpc::ChannelArguments args;
        args.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS, 100);
        args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
        channel_ = grpc::CreateCustomChannel(name_, grpc::InsecureChannelCredentials(), args);
        stub_ = cache::Cache::NewStub(channel_);
    }

    /**
     * @brief Gets the value associated with a key in a specific group.
     * @param group_name The name of the group.
     * @param key The key to look up.
     * @return An optional containing the value if found, or std::nullopt if not found.
     */
    template<typename T>
    std::optional<T> get(const std::string& group_name, const std::string& key) {
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(3));
        cache::Request request;
        request.set_group(group_name);
        request.set_key(key);
        cache::GetResponse response;
        grpc::Status status = stub_->Get(&context, request, &response);
        if (!status.ok()) {
            return std::nullopt;
        }
        if (response.has_value()) {
            T value;
            if (response.value().UnpackTo(&value)) {
                return value;
            }
            spdlog::error("Failed to unpack response value for key: {}", key);
        }
        return std::nullopt;
    }

    /**
     * @brief Sets a value for a key in a specific group.
     * @param group_name The name of the group.
     * @param key The key to set.
     * @param value The value to associate with the key.
     * @return True if the operation was successful, false otherwise.
     */
    template<typename T>
    bool set(const std::string& group_name, const std::string& key, const T& value) {
        grpc::ClientContext context;
        cache::Request request;
        request.set_group(group_name);
        request.set_key(key);

        request.mutable_value()->PackFrom(value);
        cache::SetResponse response;
        grpc::Status status = stub_->Set(&context, request, &response);
        if (!status.ok()) {
            spdlog::error("Failed to set key to peer: {}", status.error_message());
            return false;
        }
        return true;
    }

    /**
     * @brief Deletes a key from a specific group.
     * @param group_name The name of the group.
     * @param key The key to delete.
     * @return True if the operation was successful, false otherwise.
     */
    bool delete_key(const std::string& group_name, const std::string& key) {
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(3));
        cache::Request request;
        request.set_group(group_name);
        request.set_key(key);
        cache::DeleteResponse response;
        grpc::Status status = stub_->Delete(&context, request, &response);
        if (!status.ok()) {
            spdlog::error("Failed to delete key from peer: {}", status.error_message());
            return false;
        }
        return true;
    }

private:
    std::string name_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<cache::Cache::Stub> stub_;
};
#endif // peer_h