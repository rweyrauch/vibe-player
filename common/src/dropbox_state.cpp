#include "dropbox_state.h"
#include "dropbox_client.h"
#include "temp_file_manager.h"
#include <spdlog/spdlog.h>
#include <memory>
#include <mutex>

// Global state
static std::unique_ptr<DropboxClient> g_dropbox_client;
static std::unique_ptr<TempFileManager> g_temp_file_manager;
static std::mutex g_state_mutex;

void initializeDropboxSupport(const std::string& access_token)
{
    std::lock_guard<std::mutex> lock(g_state_mutex);

    if (g_dropbox_client) {
        spdlog::warn("Dropbox support already initialized");
        return;
    }

    spdlog::info("Initializing Dropbox support");

    try {
        g_dropbox_client = std::make_unique<DropboxClient>(access_token);
        g_temp_file_manager = std::make_unique<TempFileManager>();

        // Test connection
        if (!g_dropbox_client->testConnection()) {
            spdlog::error("Dropbox connection test failed: {}", g_dropbox_client->getLastError());
            g_dropbox_client.reset();
            g_temp_file_manager.reset();
            throw std::runtime_error("Failed to connect to Dropbox");
        }

        spdlog::info("Dropbox support initialized successfully");
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize Dropbox support: {}", e.what());
        g_dropbox_client.reset();
        g_temp_file_manager.reset();
        throw;
    }
}

DropboxClient* getDropboxClient()
{
    std::lock_guard<std::mutex> lock(g_state_mutex);
    return g_dropbox_client.get();
}

TempFileManager* getTempFileManager()
{
    std::lock_guard<std::mutex> lock(g_state_mutex);
    return g_temp_file_manager.get();
}

void cleanupDropboxSupport()
{
    std::lock_guard<std::mutex> lock(g_state_mutex);

    if (g_temp_file_manager) {
        spdlog::debug("Cleaning up Dropbox temp files");
        g_temp_file_manager.reset();
    }

    if (g_dropbox_client) {
        spdlog::debug("Cleaning up Dropbox client");
        g_dropbox_client.reset();
    }
}
