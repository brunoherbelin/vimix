/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/


// multiplatform implementation of system dialogs
//
// 'TinyFileDialog' from https://github.com/native-toolkit/tinyfiledialogs.git
// is the prefered solution, but it is not compatible with linux snap packaging
// because of 'zenity' access rights nightmare :(
// Thus this re-implementation of native GTK+ dialogs for linux
//
// GTK+ is not thread safe: all GTK+ calls MUST be done in the main thread.
// This is not only a matter of principle: under Wayland, the libdecor GTK
// plugin used by GLFW for window decorations draws with GTK+ in the main
// thread (during glfwPollEvents). Therefore, the GTK+ dialogs are created
// in the main thread, are not modal (no gtk_dialog_run), their result is
// obtained in the "response" signal callback, and the GTK+ events are
// processed by DialogToolkit::processEvents() called in the rendering loop.

#include <functional>
#include <thread>
#include <mutex>
#include <deque>

#include "Settings.h"
#include "SystemToolkit.h"
#include "DialogToolkit.h"

#if defined(LINUX)
#define USE_TINYFILEDIALOG 0
#else
#define USE_TINYFILEDIALOG 1
#endif

#if USE_TINYFILEDIALOG
#include <cmath>
#include "tinyfiledialogs.h"
#else
#include <gtk/gtk.h>
#endif


// globals
const std::chrono::milliseconds timeout = std::chrono::milliseconds(4);
bool DialogToolkit::FileDialog::busy_ = false;
bool DialogToolkit::yesCancelDialog::busy_ = false;

// append extension of first pattern if missing
std::string append_extension(const std::string &filename, const std::vector<std::string> &patterns)
{
    std::string f = filename;
    if (!f.empty() && !patterns.empty()) {
        std::string ext = patterns.front().substr(2);
        if (!SystemToolkit::has_extension(f, ext))
            f += std::string(".") + ext;
    }
    return f;
}

//
// FileDialog common functions
//
DialogToolkit::FileDialog::FileDialog(const std::string &name) : id_(name)
{
    if ( Settings::application.dialogRecentFolder.count(id_) == 0 )
        Settings::application.dialogRecentFolder[id_] = SystemToolkit::home_path();
}

bool DialogToolkit::FileDialog::closed()
{
    if ( !promises_.empty() ) {
        // check that file dialog thread finished
        if (promises_.back().wait_for(timeout) == std::future_status::ready ) {
            // get the filename from this file dialog
            path_ = promises_.back().get();
            if (!path_.empty()) {
                // save path location
                Settings::application.dialogRecentFolder[id_] = SystemToolkit::path_filename(path_);
            }
            // done with this file dialog
            promises_.pop_back();
            busy_ = false;
            return true;
        }
    }
    return false;
}

bool DialogToolkit::OpenManyFilesDialog::closed()
{
    if ( !promisedlist_.empty() ) {
        // check that file dialog thread finished
        if (promisedlist_.back().wait_for(timeout) == std::future_status::ready ) {
            // get the filename from this file dialog
            std::list<std::string>  list = promisedlist_.back().get();
            if (!list.empty()) {
                // selected a filenames
                pathlist_ = list;
                path_ = list.front();
                // save path location
                Settings::application.dialogRecentFolder[id_] = SystemToolkit::path_filename(path_);
            }
            else {
                pathlist_.clear();
                path_.clear();
            }
            // done with this file dialog
            promisedlist_.pop_back();
            busy_ = false;
            return true;
        }
    }
    return false;
}

void DialogToolkit::SaveFileDialog::setFolder(std::string path)
{
    Settings::application.dialogRecentFolder[id_] = SystemToolkit::path_filename( path );
}

//
// Yes/Cancel dialog common functions
//
DialogToolkit::yesCancelDialog::yesCancelDialog(const std::string &message)
    : message_(message), result_(false)
{
}

bool DialogToolkit::yesCancelDialog::closed()
{
    if ( !promises_.empty() ) {
        // check that dialog thread finished
        if (promises_.back().wait_for(timeout) == std::future_status::ready ) {
            // get the result from the dialog
            result_ = promises_.back().get();
            // done with this dialog
            promises_.pop_back();
            busy_ = false;
            return true;
        }
    }
    return false;
}

//
// Color Picker Dialog common functions
//
DialogToolkit::ColorPickerDialog::ColorPickerDialog(): busy_(false)
{
    rgb_ = std::make_tuple(0.f, 0.f, 0.f);
}


#if USE_TINYFILEDIALOG

//
//
//  TINYFILEDIALOG IMPLEMENTATION (OSX, WINDOWS)
//  Blocking dialogs run in a thread
//
//

// convert patterns list to array of char* for tinyfd
std::vector<const char *> tinyfd_patterns(const std::vector<std::string> &patterns)
{
    std::vector<const char *> p;
    for (const auto &pattern : patterns)
        p.push_back(pattern.c_str());
    return p;
}

std::string openFileDialog(const std::string &label,
                           const std::string &path,
                           const std::string &type,
                           const std::vector<std::string> patterns)
{
    std::string filename = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();

    char const *open_file_name = tinyfd_openFileDialog(label.c_str(),
                                                       startpath.c_str(),
                                                       patterns.size(),
                                                       tinyfd_patterns(patterns).data(),
                                                       type.c_str(),
                                                       0);
    if (open_file_name)
        filename = std::string(open_file_name);

    return filename;
}

std::list<std::string> selectManyFilesDialog(const std::string &label,
                                             const std::string &path,
                                             const std::string &type,
                                             const std::vector<std::string> patterns)
{
    std::list<std::string> files;
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();

    char const *open_file_names = tinyfd_openFileDialog(label.c_str(),
                                                        startpath.c_str(),
                                                        patterns.size(),
                                                        tinyfd_patterns(patterns).data(),
                                                        type.c_str(),
                                                        1);
    if (open_file_names) {
        const std::string &str(open_file_names);
        const std::string &delimiters = "|";
        // Skip delimiters at beginning.
        std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);

        // Find first non-delimiter.
        std::string::size_type pos = str.find_first_of(delimiters, lastPos);

        while (std::string::npos != pos || std::string::npos != lastPos) {
            // Found a token, add it to the vector.
            files.push_back(str.substr(lastPos, pos - lastPos));

            // Skip delimiters.
            lastPos = str.find_first_not_of(delimiters, pos);

            // Find next non-delimiter.
            pos = str.find_first_of(delimiters, lastPos);
        }
    }

    return files;
}

std::string saveFileDialog(const std::string &label,
                           const std::string &path,
                           const std::string &type,
                           const std::vector<std::string> patterns)
{
    std::string filename = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();

    char const *save_file_name = tinyfd_saveFileDialog(label.c_str(),
                                                       startpath.c_str(),
                                                       patterns.size(),
                                                       tinyfd_patterns(patterns).data(),
                                                       type.c_str());
    if (save_file_name)
        filename = std::string(save_file_name);

    return append_extension(filename, patterns);
}

std::string openFolderDialog(const std::string &label, const std::string &path)
{
    std::string foldername = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();

    char const *open_folder_name = tinyfd_selectFolderDialog(label.c_str(), startpath.c_str());
    if (open_folder_name)
        foldername = std::string(open_folder_name);

    return foldername;
}

bool yesCancelDialogCallback(const std::string &message)
{
    int ret = tinyfd_messageBox(APP_TITLE, message.c_str(), "okcancel", "warning", 0);
    return (ret == 1); // 1 = ok, 0 = no/cancel
}

void DialogToolkit::OpenFileDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        promises_.emplace_back( std::async(std::launch::async, openFileDialog, id_,
                                          Settings::application.dialogRecentFolder[id_],
                                          type_, patterns_) );
        busy_ = true;
    }
}

void DialogToolkit::OpenManyFilesDialog::open()
{
    if ( !busy_ && promisedlist_.empty() ) {
        promisedlist_.emplace_back( std::async(std::launch::async, selectManyFilesDialog, id_,
                                              Settings::application.dialogRecentFolder[id_],
                                              type_, patterns_) );
        busy_ = true;
    }
}

void DialogToolkit::SaveFileDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        promises_.emplace_back( std::async(std::launch::async, saveFileDialog, id_,
                                          Settings::application.dialogRecentFolder[id_],
                                          type_, patterns_) );
        busy_ = true;
    }
}

void DialogToolkit::OpenFolderDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        promises_.emplace_back( std::async(std::launch::async, openFolderDialog, id_,
                                           Settings::application.dialogRecentFolder[id_]) );
        busy_ = true;
    }
}

void DialogToolkit::yesCancelDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        promises_.emplace_back( std::async(std::launch::async, yesCancelDialogCallback, message_) );
        busy_ = true;
    }
}

void DialogToolkit::ErrorDialog(const char* message)
{
    tinyfd_messageBox( APP_TITLE, message, "ok", "error", 0);
}

void DialogToolkit::ColorPickerDialog::openColorDialog( std::function<void (std::tuple<float, float, float>)> func )
{
    DialogToolkit::ColorPickerDialog::instance().busy_ = true;

    // default return value to given value (so Cancel does nothing)
    std::tuple<float, float, float> rgb = DialogToolkit::ColorPickerDialog::instance().RGB();

    unsigned char prev_color[3];
    prev_color[0] = (char) ceilf(255.f * std::get<0>(rgb));
    prev_color[1] = (char) ceilf(255.f * std::get<1>(rgb));
    prev_color[2] = (char) ceilf(255.f * std::get<2>(rgb));

    unsigned char ret_color[3];

    // call the function with the selected color
    if ( NULL != tinyfd_colorChooser("Choose or pick a color", NULL, prev_color, ret_color) )
        func( { (float) ret_color[0] / 255.f, (float) ret_color[1] / 255.f, (float) ret_color[2] / 255.f} );

    DialogToolkit::ColorPickerDialog::instance().busy_ = false;
}

void DialogToolkit::ColorPickerDialog::open(std::function<void(std::tuple<float, float, float>)> func)
{
    if (!busy_)
        // launch thread to get color from dialog
        std::thread( openColorDialog, func ).detach();
}

void DialogToolkit::processEvents()
{
}

#else

//
//
//  GTK+ IMPLEMENTATION (LINUX)
//  Non-blocking dialogs, all in the main thread
//
//

// dynamic initialization of globals is done in the main thread
static const std::thread::id main_thread_id = std::this_thread::get_id();
static bool gtk_init_ok = false;

// error messages from other threads, to be shown in the main thread
static std::mutex error_queue_access;
static std::deque<std::string> error_queue;

bool gtk_init()
{
    if (!gtk_init_ok) {
        if ( gtk_init_check( NULL, NULL ) )
            gtk_init_ok = true;
    }
    return gtk_init_ok;
}

void add_filter_any_file_dialog( GtkWidget *dialog)
{
    /* append a wildcard option */
    GtkFileFilter *filter;
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name( filter, "Any file" );
    gtk_file_filter_add_pattern( filter, "*" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter );
}

void add_filter_file_dialog( GtkWidget *dialog,
                            const std::string &type,
                            const std::vector<std::string> patterns)
{
    GtkFileFilter *filter;
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name( filter, type.c_str() );
    for (size_t i = 0; i < patterns.size(); i++ ) {
        gchar *lower = g_ascii_strdown(patterns[i].c_str(), -1);
        gchar *upper = g_ascii_strup(patterns[i].c_str(), -1);
        gtk_file_filter_add_pattern( filter, lower );
        gtk_file_filter_add_pattern( filter, upper );
        g_free(lower);
        g_free(upper);
    }
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter );
}

// keep the dialog in front, at the given position if valid
void place_dialog(GtkWidget *dialog, int x, int y)
{
    gtk_window_set_keep_above( GTK_WINDOW(dialog), TRUE );
    if (x > 0 && y > 0)
        gtk_window_move( GTK_WINDOW(dialog), x, y);
}

// Show a non-blocking dialog;
// 'on_response' is called with the dialog response, and then the
// dialog is destroyed after storing its position in (*x, *y)
typedef std::function<void(GtkWidget *, gint)> ResponseCallback;
struct DialogContext
{
    ResponseCallback on_response;
    int *x;
    int *y;
};

void dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
    DialogContext *context = static_cast<DialogContext *>(user_data);

    // remember position
    gtk_window_get_position( GTK_WINDOW(dialog), context->x, context->y);

    // use response
    context->on_response( GTK_WIDGET(dialog), response_id );

    // done
    gtk_widget_destroy( GTK_WIDGET(dialog) );
    delete context;
}

void show_dialog(GtkWidget *dialog, ResponseCallback on_response, int *x, int *y)
{
    place_dialog(dialog, *x, *y);
    g_signal_connect( dialog, "response", G_CALLBACK(dialog_response),
                      new DialogContext({on_response, x, y}) );
    gtk_window_present( GTK_WINDOW(dialog) );
}

GtkWidget *file_chooser_dialog_new(const std::string &label,
                                   const std::string &path,
                                   GtkFileChooserAction action,
                                   const char *accept_label)
{
    if (!gtk_init()) {
        DialogToolkit::ErrorDialog("Could not initialize GTK+ for dialog");
        return NULL;
    }

    GtkWidget *dialog = gtk_file_chooser_dialog_new( label.c_str(), NULL, action,
                                                     "_Cancel", GTK_RESPONSE_CANCEL,
                                                     accept_label, GTK_RESPONSE_ACCEPT,  NULL );

    // Set the default path
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(dialog), startpath.c_str() );

    return dialog;
}

// show a file chooser dialog, and set the promise with the filename selected
// (with extension of first pattern appended if missing)
void show_file_chooser_dialog(GtkWidget *dialog,
                              std::shared_ptr< std::promise<std::string> > promise,
                              const std::vector<std::string> &patterns = {})
{
    if (!dialog) {
        promise->set_value("");
        return;
    }

    show_dialog(dialog, [promise, patterns](GtkWidget *d, gint response) {
            std::string filename;
            if ( response == GTK_RESPONSE_ACCEPT ) {
                char *name = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );
                if (name) {
                    filename = std::string(name);
                    g_free( name );
                }
            }
            promise->set_value( append_extension(filename, patterns) );
        },
        &Settings::application.dialogPosition.x,
        &Settings::application.dialogPosition.y);
}

void DialogToolkit::OpenFileDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        auto promise = std::make_shared< std::promise<std::string> >();
        promises_.emplace_back( promise->get_future() );
        busy_ = true;

        GtkWidget *dialog = file_chooser_dialog_new( id_, Settings::application.dialogRecentFolder[id_],
                                                     GTK_FILE_CHOOSER_ACTION_OPEN, "_Open" );
        if (dialog) {
            // set file filters
            if (!patterns_.empty())
                add_filter_file_dialog(dialog, type_, patterns_);
            add_filter_any_file_dialog(dialog);
        }

        show_file_chooser_dialog(dialog, promise);
    }
}

void DialogToolkit::OpenManyFilesDialog::open()
{
    if ( !busy_ && promisedlist_.empty() ) {
        auto promise = std::make_shared< std::promise< std::list<std::string> > >();
        promisedlist_.emplace_back( promise->get_future() );
        busy_ = true;

        GtkWidget *dialog = file_chooser_dialog_new( id_, Settings::application.dialogRecentFolder[id_],
                                                     GTK_FILE_CHOOSER_ACTION_OPEN, "_Open" );
        if (!dialog) {
            promise->set_value( {} );
            return;
        }

        // set file filters
        if (!patterns_.empty())
            add_filter_file_dialog(dialog, type_, patterns_);
        add_filter_any_file_dialog(dialog);

        // multiple files
        gtk_file_chooser_set_select_multiple( GTK_FILE_CHOOSER(dialog), true );

        show_dialog(dialog, [promise](GtkWidget *d, gint response) {
                std::list<std::string> files;
                if ( response == GTK_RESPONSE_ACCEPT ) {
                    GSList *names = gtk_file_chooser_get_filenames( GTK_FILE_CHOOSER(d) );
                    for (GSList *it = names; it != NULL; it = it->next)
                        files.push_back( (char *) it->data );
                    g_slist_free_full( names, g_free );
                }
                promise->set_value(files);
            },
            &Settings::application.dialogPosition.x,
            &Settings::application.dialogPosition.y);
    }
}

void DialogToolkit::SaveFileDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        auto promise = std::make_shared< std::promise<std::string> >();
        promises_.emplace_back( promise->get_future() );
        busy_ = true;

        GtkWidget *dialog = file_chooser_dialog_new( id_, Settings::application.dialogRecentFolder[id_],
                                                     GTK_FILE_CHOOSER_ACTION_SAVE, "_Save" );
        if (dialog) {
            gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(dialog), TRUE );

            // set file filters
            if (!patterns_.empty())
                add_filter_file_dialog(dialog, type_, patterns_);
            add_filter_any_file_dialog(dialog);
        }

        show_file_chooser_dialog(dialog, promise, patterns_);
    }
}

void DialogToolkit::OpenFolderDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        auto promise = std::make_shared< std::promise<std::string> >();
        promises_.emplace_back( promise->get_future() );
        busy_ = true;

        GtkWidget *dialog = file_chooser_dialog_new( id_, Settings::application.dialogRecentFolder[id_],
                                                     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "_Select" );

        show_file_chooser_dialog(dialog, promise);
    }
}

void DialogToolkit::yesCancelDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        auto promise = std::make_shared< std::promise<bool> >();
        promises_.emplace_back( promise->get_future() );
        busy_ = true;

        if (!gtk_init()) {
            promise->set_value(false);
            return;
        }

        GtkWidget *dialog = gtk_message_dialog_new( NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,
                                                    "%s", message_.c_str() );

        show_dialog(dialog, [promise](GtkWidget *, gint response) {
                promise->set_value( response == GTK_RESPONSE_OK );
            },
            &Settings::application.dialogPosition.x,
            &Settings::application.dialogPosition.y);
    }
}

GtkWidget *error_dialog_new(const char *message)
{
    return gtk_message_dialog_new( NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                   "Error: %s", message);
}

static int error_dialog_x = 0, error_dialog_y = 0;

void DialogToolkit::ErrorDialog(const char* message)
{
    // GTK+ cannot be used outside of main thread: show it later
    if (std::this_thread::get_id() != main_thread_id) {
        std::lock_guard<std::mutex> lock(error_queue_access);
        error_queue.push_back(message);
        return;
    }

    if (!gtk_init())
        return;

    // in main thread, show blocking dialog
    GtkWidget *dialog = error_dialog_new(message);
    place_dialog(dialog, error_dialog_x, error_dialog_y);
    gtk_dialog_run( GTK_DIALOG(dialog) );
    gtk_window_get_position( GTK_WINDOW(dialog), &error_dialog_x, &error_dialog_y);
    gtk_widget_destroy( dialog );
}

void DialogToolkit::ColorPickerDialog::open(std::function<void(std::tuple<float, float, float>)> func)
{
    if (busy_ || !gtk_init())
        return;

    busy_ = true;

    GtkWidget *dialog = gtk_color_chooser_dialog_new( "Choose or pick a color", NULL);

    // set initial color
    GdkRGBA color;
    color.red   = std::get<0>(rgb_);
    color.green = std::get<1>(rgb_);
    color.blue  = std::get<2>(rgb_);
    color.alpha = 1.f;
    gtk_color_chooser_set_rgba( GTK_COLOR_CHOOSER(dialog), &color );

    // no alpha, with editor for picking color
    gtk_color_chooser_set_use_alpha( GTK_COLOR_CHOOSER(dialog), false);
    g_object_set( GTK_WINDOW(dialog), "show-editor", TRUE, NULL );

    static int x = 0, y = 0;
    show_dialog(dialog, [this, func](GtkWidget *d, gint response) {
            // call the function with the selected color (so Cancel does nothing)
            if ( response == GTK_RESPONSE_OK ) {
                GdkRGBA c;
                gtk_color_chooser_get_rgba( GTK_COLOR_CHOOSER(d), &c );
                func( { (float) c.red, (float) c.green, (float) c.blue } );
            }
            busy_ = false;
        }, &x, &y);
}

void DialogToolkit::processEvents()
{
    // show error dialogs requested from other threads
    std::deque<std::string> errors;
    {
        std::lock_guard<std::mutex> lock(error_queue_access);
        errors.swap(error_queue);
    }
    if (!errors.empty() && gtk_init()) {
        for (const auto &message : errors)
            show_dialog( error_dialog_new(message.c_str()), [](GtkWidget *, gint) {},
                         &error_dialog_x, &error_dialog_y );
    }

    if (!gtk_init_ok)
        return;

    // process GTK+ events
    while ( gtk_events_pending() )
        gtk_main_iteration_do( FALSE );
}

#endif
