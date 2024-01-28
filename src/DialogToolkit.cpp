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

#include "defines.h"
#include "Settings.h"
#include "SystemToolkit.h"
#include "DialogToolkit.h"

#if defined(LINUX)
#define USE_TINYFILEDIALOG 0
#else
#define USE_TINYFILEDIALOG 1
#endif

#if USE_TINYFILEDIALOG
#include "tinyfiledialogs.h"
#else
#include <stdio.h>
#include <gtk/gtk.h>

static bool gtk_init_ok = false;

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
        gtk_file_filter_add_pattern( filter, g_ascii_strdown(patterns[i].c_str(), -1) );
        gtk_file_filter_add_pattern( filter, g_ascii_strup(patterns[i].c_str(), -1) );
    }
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter );
}

void wait_for_event(void)
{
    while ( gtk_events_pending() )
        gtk_main_iteration();
}

bool gtk_init()
{
    if (!gtk_init_ok) {
        if ( gtk_init_check( NULL, NULL ) )
            gtk_init_ok = true;
    }
    return gtk_init_ok;
}
#endif


// globals
const std::chrono::milliseconds timeout = std::chrono::milliseconds(4);
bool DialogToolkit::FileDialog::busy_ = false;

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

//
// open file implementation
//

std::string openFileDialog(const std::string &label,
                           const std::string &path,
                           const std::string &type,
                           const std::vector<std::string> patterns);

void DialogToolkit::OpenFileDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        promises_.emplace_back( std::async(std::launch::async, openFileDialog, id_,
                                          Settings::application.dialogRecentFolder[id_],
                                          type_, patterns_) );
        busy_ = true;
    }
}

//
// open many files implementation
//

std::list<std::string> selectManyFilesDialog(const std::string &label,
                                             const std::string &path,
                                             const std::string &type,
                                             const std::vector<std::string> patterns);
void DialogToolkit::OpenManyFilesDialog::open()
{
    if ( !busy_ && promisedlist_.empty() ) {
        promisedlist_.emplace_back( std::async(std::launch::async, selectManyFilesDialog, id_,
                                              Settings::application.dialogRecentFolder[id_],
                                              type_, patterns_) );
        busy_ = true;
    }
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

//
// save file implementation
//

std::string saveFileDialog(const std::string &label,
                           const std::string &path,
                           const std::string &type,
                           const std::vector<std::string> patterns);

void DialogToolkit::SaveFileDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        promises_.emplace_back( std::async(std::launch::async, saveFileDialog, id_,
                                          Settings::application.dialogRecentFolder[id_],
                                          type_, patterns_) );
        busy_ = true;
    }
}

void DialogToolkit::SaveFileDialog::setFolder(std::string path)
{
    Settings::application.dialogRecentFolder[id_] = SystemToolkit::path_filename( path );
}


//
// open folder implementation
//

std::string openFolderDialog(const std::string &label, const std::string &path);
void DialogToolkit::OpenFolderDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        promises_.emplace_back( std::async(std::launch::async, openFolderDialog, id_,
                                           Settings::application.dialogRecentFolder[id_]) );
        busy_ = true;
    }
}


//
//
//  CALLBACKS
//
//


std::string openFileDialog(const std::string &label,
                           const std::string &path,
                           const std::string &type,
                           const std::vector<std::string> patterns)
{
    std::string filename = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();

#if USE_TINYFILEDIALOG
    char const *open_file_name;

    // convert patterns list to array of char* for tinyfd
    char ** open_pattern = new char*[patterns.size()];
    for (size_t i = 0; i < patterns.size(); ++i) {
        open_pattern[i] = new char[patterns[i].length() + 1]; // +1 for null terminator
        std::strcpy(open_pattern[i], patterns[i].c_str());
    }

    // call tinyfd dialog
    open_file_name = tinyfd_openFileDialog( label.c_str(),
                                           startpath.c_str(),
                                           patterns.size(),
                                           open_pattern,
                                           type.c_str(),
                                           0);

    if (open_file_name)
        filename = std::string(open_file_name);
#else

    if (!gtk_init()) {
        DialogToolkit::ErrorDialog("Could not initialize GTK+ for dialog");
        return filename;
    }

    GtkWidget *dialog = gtk_file_chooser_dialog_new( label.c_str(),  NULL,
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Open", GTK_RESPONSE_ACCEPT,  NULL );

    // set file filters
    if (!patterns.empty())
        add_filter_file_dialog(dialog, type.c_str(), patterns);
    add_filter_any_file_dialog(dialog);

    // Set the default path
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(dialog), startpath.c_str() );

    // ensure front and centered
    gtk_window_set_keep_above( GTK_WINDOW(dialog), TRUE );
    if (Settings::application.dialogPosition.x > 0 && Settings::application.dialogPosition.y > 0)
        gtk_window_move( GTK_WINDOW(dialog),
                        Settings::application.dialogPosition.x,
                        Settings::application.dialogPosition.y);

    // display and get filename
    if ( gtk_dialog_run( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT ) {

        char *open_file_name = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog) );
        if (open_file_name) {
            filename = std::string(open_file_name);
            g_free( open_file_name );
        }
    }

    // remember position
    gtk_window_get_position( GTK_WINDOW(dialog),
                            &Settings::application.dialogPosition.x,
                            &Settings::application.dialogPosition.y);

    // done
    gtk_widget_destroy(dialog);
    wait_for_event();
#endif

    return filename;
}


std::list<std::string> selectManyFilesDialog(const std::string &label,
                                             const std::string &path,
                                             const std::string &type,
                                             const std::vector<std::string> patterns)
{
    std::list<std::string> files;
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();

#if USE_TINYFILEDIALOG
    char const *open_file_names;

    // convert patterns list to array of char* for tinyfd
    char **open_pattern = new char *[patterns.size()];
    for (size_t i = 0; i < patterns.size(); ++i) {
        open_pattern[i] = new char[patterns[i].length() + 1]; // +1 for null terminator
        std::strcpy(open_pattern[i], patterns[i].c_str());
    }

    // call tinyfd dialog
    open_file_names = tinyfd_openFileDialog(label.c_str(),
                                            startpath.c_str(),
                                            patterns.size(),
                                            open_pattern,
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
#else

    if (!gtk_init()) {
        DialogToolkit::ErrorDialog("Could not initialize GTK+ for dialog");
        return files;
    }

    GtkWidget *dialog = gtk_file_chooser_dialog_new(label.c_str(),
                                                    NULL,
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "_Cancel",
                                                    GTK_RESPONSE_CANCEL,
                                                    "_Open",
                                                    GTK_RESPONSE_ACCEPT,
                                                    NULL);

    // set file filters
    if (!patterns.empty())
        add_filter_file_dialog(dialog, type.c_str(), patterns);
    add_filter_any_file_dialog(dialog);

    // multiple files
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), true);

    // Set the default path
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), startpath.c_str());
    // ensure front and centered
    gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
    if (Settings::application.dialogPosition.x > 0 && Settings::application.dialogPosition.y > 0)
        gtk_window_move( GTK_WINDOW(dialog),
                        Settings::application.dialogPosition.x,
                        Settings::application.dialogPosition.y);

    // display and get filename
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        GSList *open_file_names = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
        while (open_file_names) {
            files.push_back((char *) open_file_names->data);
            open_file_names = open_file_names->next;
        }
        g_slist_free(open_file_names);
    }

    // remember position
    gtk_window_get_position( GTK_WINDOW(dialog),
                            &Settings::application.dialogPosition.x,
                            &Settings::application.dialogPosition.y);

    // done
    gtk_widget_destroy(dialog);
    wait_for_event();
#endif

    return files;
}


std::string saveFileDialog(const std::string &label,
                           const std::string &path,
                           const std::string &type,
                           const std::vector<std::string> patterns)
{
    std::string filename = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();

#if USE_TINYFILEDIALOG
    char const *save_file_name;

    // convert patterns list to array of char* for tinyfd
    char ** save_pattern = new char*[patterns.size()];
    for (size_t i = 0; i < patterns.size(); ++i) {
        save_pattern[i] = new char[patterns[i].length() + 1]; // +1 for null terminator
        std::strcpy(save_pattern[i], patterns[i].c_str());
    }

    // call tinyfd dialog
    save_file_name = tinyfd_saveFileDialog(label.c_str(),
                                           startpath.c_str(),
                                           patterns.size(),
                                           save_pattern,
                                           type.c_str()
                                           );

    if (save_file_name)
        filename = std::string(save_file_name);
#else
    if (!gtk_init()) {
        DialogToolkit::ErrorDialog("Could not initialize GTK+ for dialog");
        return filename;
    }

    GtkWidget *dialog = gtk_file_chooser_dialog_new(label.c_str(),
                                                    NULL,
                                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                                    "_Cancel",
                                                    GTK_RESPONSE_CANCEL,
                                                    "_Save",
                                                    GTK_RESPONSE_ACCEPT,
                                                    NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    // set file filters
    if (!patterns.empty())
        add_filter_file_dialog(dialog, type.c_str(), patterns);
    add_filter_any_file_dialog(dialog);

    // Set the default path
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), startpath.c_str());

    // ensure front and centered
    gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
    if (Settings::application.dialogPosition.x > 0 && Settings::application.dialogPosition.y > 0)
        gtk_window_move( GTK_WINDOW(dialog),
                        Settings::application.dialogPosition.x,
                        Settings::application.dialogPosition.y);

    // display and get filename
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *save_file_name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (save_file_name) {
            filename = std::string(save_file_name);
            g_free(save_file_name);
        }
    }

    // remember position
    gtk_window_get_position( GTK_WINDOW(dialog),
                            &Settings::application.dialogPosition.x,
                            &Settings::application.dialogPosition.y);

    // done
    gtk_widget_destroy(dialog);
    wait_for_event();
#endif

    // append extension of first pattern if missing
    if (!filename.empty() && !patterns.empty()) {
        std::string ext = patterns.front().substr(2);
        if (!SystemToolkit::has_extension(filename, ext ) )
            filename += std::string(".") + ext;
    }

    return filename;
}


std::string openFolderDialog(const std::string &label, const std::string &path)
{
    std::string foldername = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();

#if USE_TINYFILEDIALOG
    char const * open_folder_name;
    open_folder_name = tinyfd_selectFolderDialog(label.c_str(), startpath.c_str());

    if (open_folder_name)
        foldername = std::string(open_folder_name);
#else
    if (!gtk_init()) {
        DialogToolkit::ErrorDialog("Could not initialize GTK+ for dialog");
        return foldername;
    }

    GtkWidget *dialog = gtk_file_chooser_dialog_new( label.c_str(), NULL,
                                                     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                     "_Cancel", GTK_RESPONSE_CANCEL,
                                                     "_Select", GTK_RESPONSE_ACCEPT,  NULL );
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(dialog), TRUE );

    // Set the default path
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(dialog), startpath.c_str() );

    // ensure front and centered
    gtk_window_set_keep_above( GTK_WINDOW(dialog), TRUE );
    if (Settings::application.dialogPosition.x > 0 && Settings::application.dialogPosition.y > 0)
        gtk_window_move( GTK_WINDOW(dialog),
                        Settings::application.dialogPosition.x,
                        Settings::application.dialogPosition.y);

    // display and get filename
    if ( gtk_dialog_run( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT ) {

        char *open_folder_name = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog) );
        if (open_folder_name) {
            foldername = std::string(open_folder_name);
            g_free( open_folder_name );
        }
    }

    // remember position
    gtk_window_get_position( GTK_WINDOW(dialog),
                            &Settings::application.dialogPosition.x,
                            &Settings::application.dialogPosition.y);

    // done
    gtk_widget_destroy(dialog);
    wait_for_event();
#endif

    return foldername;
}


void DialogToolkit::ErrorDialog(const char* message)
{
#if USE_TINYFILEDIALOG
    tinyfd_messageBox( APP_TITLE, message, "ok", "error", 0);
#else
    if (!gtk_init()) {
        return;
    }

    GtkWidget *dialog = gtk_message_dialog_new( NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                                "Error: %s", message);

    gtk_window_set_keep_above( GTK_WINDOW(dialog), TRUE );
    static int x = 0, y = 0;
    if (x != 0)
        gtk_window_move( GTK_WINDOW(dialog), x, y);

    // show
    gtk_dialog_run( GTK_DIALOG(dialog) );

    // remember position
    gtk_window_get_position( GTK_WINDOW(dialog), &x, &y);

    // done
    gtk_widget_destroy( dialog );
    wait_for_event();
#endif
}


//
// Color Picker Dialog common functions
//

bool DialogToolkit::ColorPickerDialog::busy_ = false;

DialogToolkit::ColorPickerDialog::ColorPickerDialog()
{
    rgb_ = std::make_tuple(0.f, 0.f, 0.f);
}

bool DialogToolkit::ColorPickerDialog::closed()
{
    if ( !promises_.empty() ) {
        // check that file dialog thread finished
        if (promises_.back().wait_for(timeout) == std::future_status::ready ) {
            // get the filename from this file dialog
            rgb_ = promises_.back().get();
            // done with this file dialog
            promises_.pop_back();
            busy_ = false;
            return true;
        }
    }
    return false;
}

std::tuple<float, float, float> openColorDialog( std::tuple<float, float, float> rgb)
{
    // default return value to given value (so Cancel does nothing)
    std::tuple<float, float, float> ret = rgb;

#if USE_TINYFILEDIALOG

    unsigned char prev_color[3];
    prev_color[0] = (char) ceilf(255.f * std::get<0>(rgb));
    prev_color[1] = (char) ceilf(255.f * std::get<1>(rgb));
    prev_color[2] = (char) ceilf(255.f * std::get<2>(rgb));

    unsigned char ret_color[3];

    if ( NULL != tinyfd_colorChooser("Choose or pick a color", NULL, prev_color, ret_color) )
    {
        ret = { (float) ret_color[0] / 255.f, (float) ret_color[1] / 255.f, (float) ret_color[2] / 255.f};
    }

#else
    if (!gtk_init()) {
        return ret;
    }

    GtkWidget *dialog = gtk_color_chooser_dialog_new( "Choose or pick a color", NULL);

    // set initial color
    GdkRGBA color;
    color.red   = std::get<0>(rgb);
    color.green = std::get<1>(rgb);
    color.blue  = std::get<2>(rgb);
    color.alpha = 1.f;
    gtk_color_chooser_set_rgba( GTK_COLOR_CHOOSER(dialog), &color );

    // no alpha, with editor for picking color
    gtk_color_chooser_set_use_alpha( GTK_COLOR_CHOOSER(dialog), false);
    g_object_set( GTK_WINDOW(dialog), "show-editor", TRUE, NULL );

    // prepare dialog
    gtk_window_set_keep_above( GTK_WINDOW(dialog), TRUE );
    static int x = 0, y = 0;
    if (x != 0)
        gtk_window_move( GTK_WINDOW(dialog), x, y);

    // display and get color
    if ( gtk_dialog_run( GTK_DIALOG(dialog) ) == GTK_RESPONSE_OK ) {

        gtk_color_chooser_get_rgba( GTK_COLOR_CHOOSER(dialog), &color );
        ret = { color.red, color.green, color.blue};
    }

    // remember position
    gtk_window_get_position( GTK_WINDOW(dialog), &x, &y);

    // done
    gtk_widget_destroy( dialog );
    wait_for_event();

#endif

    return ret;
}

void DialogToolkit::ColorPickerDialog::open()
{
    if ( !DialogToolkit::ColorPickerDialog::busy_ && promises_.empty() ) {
        promises_.emplace_back( std::async(std::launch::async, openColorDialog, rgb_) );
        busy_ = true;
    }
}

