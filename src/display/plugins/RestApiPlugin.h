#pragma once
#ifndef RESTAPIPLUGIN_H
#define RESTAPIPLUGIN_H

#include <ESPAsyncWebServer.h>
#include <display/core/Plugin.h>

class RestApiPlugin : public Plugin {
  public:
    RestApiPlugin();
    void setup(Controller *controller, PluginManager *pluginManager) override;
    void loop() override;

  private:
    bool checkApiKey(AsyncWebServerRequest *request) const;
    void setupRoutes();
    void start();
    void stop();

    AsyncWebServer server;
    Controller *controller = nullptr;
    PluginManager *pluginManager = nullptr;
    bool serverRunning = false;
};

#endif // RESTAPIPLUGIN_H
