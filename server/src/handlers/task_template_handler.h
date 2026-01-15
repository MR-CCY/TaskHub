#pragma once

#include "httplib.h"
#include <memory>
namespace taskhub {

class TaskTemplateHandler {
public:
  /// Register task template HTTP routes
  static void setup_routes(httplib::Server &server);

private:
  static void create(const httplib::Request &req, httplib::Response &res);
  static void get(const httplib::Request &req, httplib::Response &res);
  static void list(const httplib::Request &req, httplib::Response &res);
  static void delete_(const httplib::Request &req, httplib::Response &res);
};

} // namespace taskhub
