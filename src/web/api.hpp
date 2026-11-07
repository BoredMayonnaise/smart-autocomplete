#pragma once
#include "smartautocomplete/core.hpp"
#include <crow/json.h>
#include <memory>
#include <mutex>
#include <semaphore>
namespace smart {
struct Reply { int status; std::string body; };
class Api {
    Limits limits_;
    mutable std::mutex mutex_;
    std::shared_ptr<const Dictionary> dictionary_;
    std::counting_semaphore<2> slots_{2};
    std::shared_ptr<const Dictionary> snapshot() const;
    void publish(std::shared_ptr<const Dictionary>, std::shared_ptr<Dictionary>);
    crow::json::wvalue route(std::string_view, std::string_view, const crow::json::rvalue&);
public:
    explicit Api(const std::vector<std::string>& words, Limits limits = {});
    Reply handle(std::string_view method, std::string_view path, std::string_view body = "");
};
}
