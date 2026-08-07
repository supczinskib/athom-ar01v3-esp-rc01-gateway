#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

using StringRef = std::string_view;
enum http_method { HTTP_GET = 0, HTTP_POST = 1 };
class AsyncWebServerResponse {
 public:
  void addHeader(const char *, const char *) {}
};
class AsyncWebServerRequest {
 public:
  static constexpr size_t URL_BUF_SIZE = 256;
  http_method method() const { return method_; }
  StringRef url_to(char (&buffer)[URL_BUF_SIZE]) const {
    const size_t n = url_.copy(buffer, URL_BUF_SIZE - 1);
    buffer[n] = 0;
    return {buffer, n};
  }
  bool hasArg(const char *name) { return args_.find(name) != args_.end(); }
  std::string arg(const char *name) {
    auto it = args_.find(name);
    return it == args_.end() ? std::string{} : it->second;
  }
  AsyncWebServerResponse *beginResponse(int, const char *, const std::string &) { return new AsyncWebServerResponse; }
  AsyncWebServerResponse *beginResponse(int, const char *, const uint8_t *, size_t) { return new AsyncWebServerResponse; }
  void send(AsyncWebServerResponse *response) { delete response; }
  void send(int, const char * = nullptr, const char * = nullptr) {}
  http_method method_{HTTP_GET};
  std::string url_;
  std::unordered_map<std::string, std::string> args_;
};
class AsyncWebHandler {
 public:
  virtual ~AsyncWebHandler() = default;
  virtual bool canHandle(AsyncWebServerRequest *) const { return false; }
  virtual void handleRequest(AsyncWebServerRequest *) {}
  virtual void handleBody(AsyncWebServerRequest *, uint8_t *, size_t, size_t, size_t) {}
  virtual void handleUpload(AsyncWebServerRequest *, const std::string &, size_t, uint8_t *, size_t, bool) {}
  virtual bool isRequestHandlerTrivial() const { return true; }
};
namespace esphome::web_server_base {
class WebServerBase { public: void add_handler(AsyncWebHandler *) {} };
}
