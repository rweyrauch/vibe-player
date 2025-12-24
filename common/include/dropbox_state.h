#ifndef DROPBOX_STATE_H
#define DROPBOX_STATE_H

#include <string>

// Forward declarations
class DropboxClient;
class TempFileManager;

// Initialize Dropbox support with access token
void initializeDropboxSupport(const std::string& access_token);

// Get global Dropbox client instance (returns nullptr if not initialized)
DropboxClient* getDropboxClient();

// Get global temp file manager instance (returns nullptr if not initialized)
TempFileManager* getTempFileManager();

// Cleanup Dropbox support (called on exit)
void cleanupDropboxSupport();

#endif // DROPBOX_STATE_H
