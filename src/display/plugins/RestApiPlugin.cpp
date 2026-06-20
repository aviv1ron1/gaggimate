#include "RestApiPlugin.h"
#include <ArduinoJson.h>
#include <display/core/Controller.h>
#include <display/core/PluginManager.h>
#include <display/core/constants.h>
#include <display/models/profile.h>

RestApiPlugin::RestApiPlugin() : server(8080) {}

void RestApiPlugin::setup(Controller *_controller, PluginManager *_pluginManager) {
    controller = _controller;
    pluginManager = _pluginManager;
    pluginManager->on("controller:wifi:connect", [this](Event const &) { start(); });
    pluginManager->on("controller:wifi:disconnect", [this](Event const &) { stop(); });
    setupRoutes();
}

void RestApiPlugin::loop() {}

bool RestApiPlugin::checkApiKey(AsyncWebServerRequest *request) const {
    if (!request->hasHeader("X-API-Key")) {
        return false;
    }
    return request->getHeader("X-API-Key")->value() == controller->getSettings().getApiKey();
}

void RestApiPlugin::setupRoutes() {
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!checkApiKey(request)) {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        doc["temperature"] = controller->getCurrentTemp();
        doc["ready"] = controller->isReady();
        doc["activeBrew"] = controller->isActive() && controller->getMode() == MODE_BREW;
        doc["power"] = controller->getMode() != MODE_STANDBY;
        doc["activeProfileId"] = controller->getProfileManager()->getSelectedProfile().id;
        serializeJson(doc, *response);
        request->send(response);
    });

    server.on("/api/brew/start", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!checkApiKey(request)) {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        if (controller->getMode() == MODE_STANDBY) {
            request->send(400, "application/json", "{\"error\":\"machine is in standby\"}");
            return;
        }
        if (controller->isActive() && controller->getMode() == MODE_BREW) {
            request->send(400, "application/json", "{\"error\":\"already brewing\"}");
            return;
        }
        controller->activate();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/api/brew/stop", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!checkApiKey(request)) {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        controller->deactivate();
        controller->clear();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/api/power/on", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!checkApiKey(request)) {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        controller->deactivateStandby();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/api/power/off", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!checkApiKey(request)) {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        controller->activateStandby();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/api/profiles", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!checkApiKey(request)) {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        auto arr = doc.to<JsonArray>();
        ProfileManager *pm = controller->getProfileManager();
        for (auto const &id : pm->listProfiles()) {
            Profile profile{};
            pm->loadProfile(id, profile);
            auto p = arr.add<JsonObject>();
            writeProfile(p, profile);
        }
        serializeJson(doc, *response);
        request->send(response);
    });

    server.on("/api/profile", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!checkApiKey(request)) {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        // Extract id from URL: /api/profile/{id}/activate
        String url = request->url();
        static const char prefix[] = "/api/profile/";
        static const int prefixLen = sizeof(prefix) - 1;
        int prefixIdx = url.indexOf(prefix);
        if (prefixIdx == -1) {
            request->send(404, "application/json", "{\"error\":\"not found\"}");
            return;
        }
        int idStart = prefixIdx + prefixLen;
        int activateIdx = url.indexOf("/activate", idStart);
        if (activateIdx == -1) {
            request->send(404, "application/json", "{\"error\":\"not found\"}");
            return;
        }
        String id = url.substring(idStart, activateIdx);
        ProfileManager *pm = controller->getProfileManager();
        if (!pm->profileExists(id)) {
            request->send(404, "application/json", "{\"error\":\"not found\"}");
            return;
        }
        pm->selectProfile(id);
        request->send(200, "application/json", "{\"ok\":true}");
    });
}

void RestApiPlugin::start() {
    if (serverRunning)
        return;
    server.begin();
    serverRunning = true;
}

void RestApiPlugin::stop() {
    if (!serverRunning)
        return;
    server.end();
    serverRunning = false;
}
