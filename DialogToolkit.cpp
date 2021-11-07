
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
static int window_x = -1, window_y = -1;

void add_filter_any_file_dialog( GtkWidget *dialog)
{
    /* append a wildcard option */
    GtkFileFilter *filter;
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name( filter, "Any file" );
    gtk_file_filter_add_pattern( filter, "*" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter );
}

void add_filter_file_dialog( GtkWidget *dialog, int const countfilterPatterns, char const * const * const filterPatterns, char const * const filterDescription)
{
    GtkFileFilter *filter;
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name( filter, filterDescription );
    for (int i = 0; i < countfilterPatterns; i++ )
        gtk_file_filter_add_pattern( filter, filterPatterns[i] );
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
// type specific implementations
//
std::string openImageFileDialog(const std::string &label, const std::string &path);
void DialogToolkit::OpenImageDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        promises_.emplace_back( std::async(std::launch::async, openImageFileDialog, id_,
                                           Settings::application.dialogRecentFolder[id_]) );
        busy_ = true;
    }
}

std::string openSessionFileDialog(const std::string &label, const std::string &path);
void DialogToolkit::OpenSessionDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        promises_.emplace_back( std::async(std::launch::async, openSessionFileDialog, id_,
                                           Settings::application.dialogRecentFolder[id_]) );
        busy_ = true;
    }
}

std::string openMediaFileDialog(const std::string &label, const std::string &path);
void DialogToolkit::OpenMediaDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        promises_.emplace_back( std::async(std::launch::async, openMediaFileDialog, id_,
                                           Settings::application.dialogRecentFolder[id_]) );
        busy_ = true;
    }
}

std::string saveSessionFileDialog(const std::string &label, const std::string &path);
void DialogToolkit::SaveSessionDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        promises_.emplace_back( std::async(std::launch::async, saveSessionFileDialog, id_,
                                           Settings::application.dialogRecentFolder[id_]) );
        busy_ = true;
    }
}

std::string openFolderDialog(const std::string &label, const std::string &path);
void DialogToolkit::OpenFolderDialog::open()
{
    if ( !busy_ && promises_.empty() ) {
        promises_.emplace_back( std::async(std::launch::async, openFolderDialog, id_,
                                           Settings::application.dialogRecentFolder[id_]) );
        busy_ = true;
    }
}

std::list<std::string> selectImagesFileDialog(const std::string &label,const std::string &path);
void DialogToolkit::MultipleImagesDialog::open()
{
    if ( !busy_ && promisedlist_.empty() ) {
        promisedlist_.emplace_back( std::async(std::launch::async, selectImagesFileDialog, id_,
                                           Settings::application.dialogRecentFolder[id_]) );
        busy_ = true;
    }
}

bool DialogToolkit::MultipleImagesDialog::closed()
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
//
//  CALLBACKS
//
//

std::string saveSessionFileDialog(const std::string &label, const std::string &path)
{
    std::string filename = "";
    char const * save_pattern[2] = { "*.mix", "*.MIX" };

#if USE_TINYFILEDIALOG
    char const * save_file_name;

    save_file_name = tinyfd_saveFileDialog( label.c_str(), path.c_str(), 2, save_pattern, "vimix session");

    if (save_file_name)
        filename = std::string(save_file_name);
#else
    if (!gtk_init()) {
        DialogToolkit::ErrorDialog("Could not initialize GTK+ for dialog");
        return filename;
    }

    GtkWidget *dialog = gtk_file_chooser_dialog_new( label.c_str(), NULL,
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          "_Cancel", GTK_RESPONSE_CANCEL,
                                          "_Save", GTK_RESPONSE_ACCEPT, NULL );
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(dialog), TRUE );

    // set file filters
    add_filter_file_dialog(dialog, 2, save_pattern, "vimix session");
    add_filter_any_file_dialog(dialog);

    // Set the default path
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(dialog), path.c_str() );

    // ensure front and centered
    gtk_window_set_keep_above( GTK_WINDOW(dialog), TRUE );
    if (window_x > 0 && window_y > 0)
        gtk_window_move( GTK_WINDOW(dialog), window_x, window_y);

    // display and get filename
    if ( gtk_dialog_run( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT ) {

        char *save_file_name = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog) );
        if (save_file_name)
            filename = std::string(save_file_name);
        g_free( save_file_name );
    }

    // remember position
    gtk_window_get_position( GTK_WINDOW(dialog), &window_x, &window_y);

    // done
    gtk_widget_destroy(dialog);
    wait_for_event();
#endif

    std::string extension = filename.substr(filename.find_last_of(".") + 1);
    if (!filename.empty() && extension != "mix")
        filename += ".mix";

    return filename;
}


std::string openSessionFileDialog(const std::string &label, const std::string &path)
{
    std::string filename = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();
    char const * open_pattern[2] = { "*.mix", "*.MIX" };

#if USE_TINYFILEDIALOG
    char const * open_file_name;
    open_file_name = tinyfd_openFileDialog( label.c_str(), startpath.c_str(), 2, open_pattern, "vimix session", 0);

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
    add_filter_file_dialog(dialog, 2, open_pattern, "vimix session");
    add_filter_any_file_dialog(dialog);

    // Set the default path
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(dialog), startpath.c_str() );

    // ensure front and centered
    gtk_window_set_keep_above( GTK_WINDOW(dialog), TRUE );
    if (window_x > 0 && window_y > 0)
        gtk_window_move( GTK_WINDOW(dialog), window_x, window_y);

    // display and get filename
    if ( gtk_dialog_run( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT ) {

        char *open_file_name = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog) );
        if (open_file_name)
            filename = std::string(open_file_name);
        g_free( open_file_name );
    }

    // remember position
    gtk_window_get_position( GTK_WINDOW(dialog), &window_x, &window_y);

    // done
    gtk_widget_destroy(dialog);
    wait_for_event();
#endif

    return filename;
}


std::string openMediaFileDialog(const std::string &label, const std::string &path)
{
    std::string filename = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();
    char const * open_pattern[52] = { "*.mix", "*.mp4", "*.mpg", "*.mpeg", "*.m2v", "*.m4v", "*.avi", "*.mov",
                                      "*.mkv", "*.webm", "*.mod", "*.wmv", "*.mxf", "*.ogg",
                                      "*.flv", "*.hevc", "*.asf", "*.jpg", "*.png",  "*.gif",
                                      "*.tif", "*.tiff", "*.webp", "*.bmp", "*.ppm", "*.svg,"
                                      "*.MIX", "*.MP4", "*.MPG", "*.MPEG","*.M2V", "*.M4V", "*.AVI", "*.MOV",
                                      "*.MKV", "*.WEBM", "*.MOD", "*.WMV", "*.MXF", "*.OGG",
                                      "*.FLV", "*.HEVC", "*.ASF", "*.JPG", "*.PNG",  "*.GIF",
                                      "*.TIF", "*.TIFF", "*.WEBP", "*.BMP", "*.PPM", "*.SVG"  };
#if USE_TINYFILEDIALOG
    char const * open_file_name;
    open_file_name = tinyfd_openFileDialog( label.c_str(), startpath.c_str(), 18, open_pattern, "All supported formats", 0);

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
    add_filter_file_dialog(dialog, 52, open_pattern, "Supported formats (videos, images, sessions)");
    add_filter_any_file_dialog(dialog);

    // Set the default path
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(dialog), startpath.c_str() );

    // ensure front and centered
    gtk_window_set_keep_above( GTK_WINDOW(dialog), TRUE );
    if (window_x > 0 && window_y > 0)
        gtk_window_move( GTK_WINDOW(dialog), window_x, window_y);

    // display and get filename
    if ( gtk_dialog_run( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT ) {

        char *open_file_name = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog) );
        if (open_file_name)
            filename = std::string(open_file_name);
        g_free( open_file_name );
    }

    // remember position
    gtk_window_get_position( GTK_WINDOW(dialog), &window_x, &window_y);

    // done
    gtk_widget_destroy(dialog);
    wait_for_event();
#endif

    return filename;
}


std::string openImageFileDialog(const std::string &label, const std::string &path)
{
    std::string filename = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();
    char const * open_pattern[10] = { "*.jpg", "*.png", "*.bmp", "*.ppm", "*.gif",
                                      "*.JPG", "*.PNG", "*.BMP", "*.PPM", "*.GIF"};
#if USE_TINYFILEDIALOG
    char const * open_file_name;
    open_file_name = tinyfd_openFileDialog( label.c_str(), startpath.c_str(), 4, open_pattern, "Image JPG or PNG", 0);

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
    add_filter_file_dialog(dialog, 10, open_pattern, "Image (JPG, PNG, BMP, PPM, GIF)");
    add_filter_any_file_dialog(dialog);

    // Set the default path
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(dialog), startpath.c_str() );

    // ensure front and centered
    gtk_window_set_keep_above( GTK_WINDOW(dialog), TRUE );
    if (window_x > 0 && window_y > 0)
        gtk_window_move( GTK_WINDOW(dialog), window_x, window_y);

    // display and get filename
    if ( gtk_dialog_run( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT ) {

        char *open_file_name = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog) );
        if (open_file_name)
            filename = std::string(open_file_name);
        g_free( open_file_name );
    }

    // remember position
    gtk_window_get_position( GTK_WINDOW(dialog), &window_x, &window_y);

    // done
    gtk_widget_destroy(dialog);
    wait_for_event();
#endif

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
    if (window_x > 0 && window_y > 0)
        gtk_window_move( GTK_WINDOW(dialog), window_x, window_y);

    // display and get filename
    if ( gtk_dialog_run( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT ) {

        char *open_folder_name = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog) );
        if (open_folder_name)
            foldername = std::string(open_folder_name);
        g_free( open_folder_name );
    }

    // remember position
    gtk_window_get_position( GTK_WINDOW(dialog), &window_x, &window_y);

    // done
    gtk_widget_destroy(dialog);
    wait_for_event();
#endif

    return foldername;
}


std::list<std::string> selectImagesFileDialog(const std::string &label,const std::string &path)
{
    std::list<std::string> files;

    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();
    char const * open_pattern[6] = {  "*.jpg", "*.png", "*.tif",
                                      "*.JPG", "*.PNG", "*.TIF"  };

#if USE_TINYFILEDIALOG
    char const * open_file_names;
    open_file_names = tinyfd_openFileDialog(label.c_str(), startpath.c_str(), 6, open_pattern, "Images", 1);

    if (open_file_names) {

        const std::string& str (open_file_names);
        const std::string& delimiters = "|";
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

    GtkWidget *dialog = gtk_file_chooser_dialog_new( label.c_str(),  NULL,
                                              GTK_FILE_CHOOSER_ACTION_OPEN,
                                              "_Cancel", GTK_RESPONSE_CANCEL,
                                              "_Open", GTK_RESPONSE_ACCEPT,  NULL );

    // set file filters
    add_filter_file_dialog(dialog, 6, open_pattern, "Images (JPG, PNG, TIF)");
    add_filter_any_file_dialog(dialog);

    // multiple files
    gtk_file_chooser_set_select_multiple( GTK_FILE_CHOOSER(dialog), true );

    // Set the default path
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(dialog), startpath.c_str() );

    // ensure front and centered
    gtk_window_set_keep_above( GTK_WINDOW(dialog), TRUE );
    if (window_x > 0 && window_y > 0)
        gtk_window_move( GTK_WINDOW(dialog), window_x, window_y);

    // display and get filename
    if ( gtk_dialog_run( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT ) {

        GSList *open_file_names = gtk_file_chooser_get_filenames( GTK_FILE_CHOOSER(dialog) );

        while (open_file_names) {
            files.push_back( (char *) open_file_names->data );
            open_file_names = open_file_names->next;
//            g_free( open_file_names->data );
        }

        g_slist_free( open_file_names );
    }

    // remember position
    gtk_window_get_position( GTK_WINDOW(dialog), &window_x, &window_y);

    // done
    gtk_widget_destroy(dialog);
    wait_for_event();
#endif


    return files;
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


