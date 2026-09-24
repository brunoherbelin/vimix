#ifndef FREEDESKTOPTOOLKIT_H
#define FREEDESKTOPTOOLKIT_H

#include <cstdint>
#include <functional>
#include <string>

/**
 * @brief The FreedesktopToolkit namespace gives access to org.freedesktop
 * services of the desktop, through D-Bus.
 *
 * All D-Bus communication happens in a dedicated thread; requests never block
 * the caller, and dialogs of the desktop do not block rendering.
 */
namespace FreedesktopToolkit
{

//
// Screen cast portal (org.freedesktop.portal.ScreenCast), which lets the
// user choose a screen or a window to capture with pipewiresrc under Wayland
//

// types of source (bitmask, as in the portal specification)
#define SCREENCAST_MONITOR 1
#define SCREENCAST_WINDOW  2
#define SCREENCAST_VIRTUAL 4

struct ScreenCastResult
{
    bool success = false;
    std::string error;          // why it failed (empty if cancelled by user)
    std::string session;        // portal session handle, to close it
    int fd = -1;                // PipeWire remote for pipewiresrc 'fd'
    uint32_t node = 0;          // PipeWire node for pipewiresrc 'path'
    uint32_t source_type = 0;   // SCREENCAST_MONITOR, _WINDOW or _VIRTUAL
    int width = 0;
    int height = 0;
    std::string restore_token;  // to restore the same selection without dialog
};

typedef std::function<void(const ScreenCastResult &)> ScreenCastCallback;

// true if the desktop provides the screen cast portal (blocking on first call)
bool screencastAvailable();

// ask the user to select a screen or a window; if restore_token is given,
// the portal may restore that previous selection without showing the dialog.
// Requests are queued and handled one at a time; the callback is called
// from the portal thread once the request is completed (or failed)
void screencastRequest(const std::string &restore_token, ScreenCastCallback callback);

// close a portal session (ends the screen cast)
void screencastClose(const std::string &session);

// set a callback called (from the portal thread) when a session is closed
// by the desktop (e.g. user clicked on 'stop sharing')
void setScreencastClosedCallback(std::function<void(const std::string &session)> callback);

}

#endif // FREEDESKTOPTOOLKIT_H
