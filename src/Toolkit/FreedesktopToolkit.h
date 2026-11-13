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

//
// Settings portal (org.freedesktop.portal.Settings), which gives read access
// to the desktop settings (e.g. 'org.gnome.desktop.interface' 'cursor-theme'),
// also from inside a Flatpak sandbox. Synchronous calls (short timeout),
// meant to be used once at startup.
//

// read a string setting (empty if not available)
std::string settingString(const std::string &ns, const std::string &key);

// read an integer setting (default_value if not available)
int settingInt(const std::string &ns, const std::string &key, int default_value = 0);

//
// Media player remote control (MPRIS, org.mpris.MediaPlayer2), which lets the
// desktop control vimix: media keys, media controls of GNOME or KDE, playerctl.
// Requests of the desktop are queued as commands, to be executed by the
// application in its main loop; the application informs of its state.
//

enum MprisAction {
    MPRIS_PLAY_PAUSE = 0,
    MPRIS_PLAY,
    MPRIS_PAUSE,
    MPRIS_NEXT,
    MPRIS_PREVIOUS,
    MPRIS_SEEK,         // relative move by 'position'
    MPRIS_SET_POSITION, // absolute move to 'position'
    MPRIS_RAISE
};

struct MprisCommand
{
    MprisAction action = MPRIS_PLAY_PAUSE;
    int64_t position = 0;       // microseconds (offset for MPRIS_SEEK)
};

struct MprisState
{
    bool playing = false;
    std::string title;          // name of what is played
    std::string artist;         // shown as artist (e.g. name of the media file)
    bool seekable = false;      // true if position and duration are known
    int64_t position = 0;       // microseconds
    int64_t duration = 0;       // microseconds
};

// publish vimix on the session bus; desktop_entry is the name of the desktop
// file without extension (for its icon and name)
void mprisStart(const std::string &identity, const std::string &desktop_entry);

// get the next command requested by the desktop; false if there is none
bool mprisCommand(MprisCommand &command);

// inform of the current state (to be called regularly, e.g. every frame;
// the desktop is notified only of changes)
void mprisSetState(const MprisState &state);

// inform that the position changed discontinuously (e.g. after a seek)
void mprisSeeked(int64_t position);

}

#endif // FREEDESKTOPTOOLKIT_H
