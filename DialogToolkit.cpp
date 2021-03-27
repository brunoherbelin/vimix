
// multiplatform implementation of system dialogs
//
// 'TinyFileDialog' from https://github.com/native-toolkit/tinyfiledialogs.git
// is the prefered solution, but it is not compatible with linux snap packaging
// because of 'zenity' access rights nightmare :(
// Thus this re-implementation of native GTK+ dialogs for linux

#if defined(LINUX)
#define USE_TINYFILEDIALOG 0
#else
#define USE_TINYFILEDIALOG 1
#endif

#include "defines.h"
#include "SystemToolkit.h"
#include "DialogToolkit.h"

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

std::string DialogToolkit::saveSessionFileDialog(const std::string &path)
{
    std::string filename = "";
    char const * save_pattern[1] = { "*.mix" };

#if USE_TINYFILEDIALOG
    char const * save_file_name;

    save_file_name = tinyfd_saveFileDialog( "Save a session file", path.c_str(), 1, save_pattern, "vimix session");

    if (save_file_name) {
        filename = std::string(save_file_name);
        std::string extension = filename.substr(filename.find_last_of(".") + 1);
        if (extension != "mix")
            filename += ".mix";
    }
#else
    if (!gtk_init()) {
        ErrorDialog("Could not initialize GTK+ for dialog");
        return filename;
    }

    GtkWidget *dialog = gtk_file_chooser_dialog_new( "Save Session File", NULL,
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          "_Cancel", GTK_RESPONSE_CANCEL,
                                          "_Save", GTK_RESPONSE_ACCEPT, NULL );
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(dialog), TRUE );

    // set file filters
    add_filter_file_dialog(dialog, 1, save_pattern, "vimix session");
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
#endif

    return filename;
}


std::string DialogToolkit::openSessionFileDialog(const std::string &path)
{
    std::string filename = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();
    char const * open_pattern[1] = { "*.mix" };

#if USE_TINYFILEDIALOG
    char const * open_file_name;
    open_file_name = tinyfd_openFileDialog( "Import a file", startpath.c_str(), 1, open_pattern, "vimix session", 0);

    if (open_file_name)
        filename = std::string(open_file_name);
#else
    if (!gtk_init()) {
        ErrorDialog("Could not initialize GTK+ for dialog");
        return filename;
    }

    GtkWidget *dialog = gtk_file_chooser_dialog_new( "Open Session File",  NULL,
                                              GTK_FILE_CHOOSER_ACTION_OPEN,
                                              "_Cancel", GTK_RESPONSE_CANCEL,
                                              "_Open", GTK_RESPONSE_ACCEPT,  NULL );

    // set file filters
    add_filter_file_dialog(dialog, 1, open_pattern, "vimix session");
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
#endif

    return filename;
}


std::string DialogToolkit::ImportFileDialog(const std::string &path)
{
    std::string filename = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();
    char const * open_pattern[18] = { "*.mix", "*.mp4", "*.mpg",
                                      "*.avi", "*.mov", "*.mkv",
                                      "*.webm", "*.mod", "*.wmv",
                                      "*.mxf", "*.ogg", "*.flv",
                                      "*.asf", "*.jpg", "*.png",
                                      "*.gif", "*.tif", "*.svg" };
#if USE_TINYFILEDIALOG
    char const * open_file_name;
    open_file_name = tinyfd_openFileDialog( "Import a file", startpath.c_str(), 18, open_pattern, "All supported formats", 0);

    if (open_file_name)
        filename = std::string(open_file_name);
#else

    if (!gtk_init()) {
        ErrorDialog("Could not initialize GTK+ for dialog");
        return filename;
    }

    GtkWidget *dialog = gtk_file_chooser_dialog_new( "Import Media File",  NULL,
                                              GTK_FILE_CHOOSER_ACTION_OPEN,
                                              "_Cancel", GTK_RESPONSE_CANCEL,
                                              "_Open", GTK_RESPONSE_ACCEPT,  NULL );

    // set file filters
    add_filter_file_dialog(dialog, 18, open_pattern, "All supported formats");
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
#endif

    return filename;
}

std::string DialogToolkit::FolderDialog(const std::string &path)
{
    std::string foldername = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();

#if USE_TINYFILEDIALOG
    char const * open_folder_name;
    open_folder_name = tinyfd_selectFolderDialog("Select folder", startpath.c_str());

    if (open_folder_name)
        foldername = std::string(open_folder_name);
#else
    if (!gtk_init()) {
        ErrorDialog("Could not initialize GTK+ for dialog");
        return foldername;
    }

    GtkWidget *dialog = gtk_file_chooser_dialog_new( "Select folder", NULL,
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
#endif
}


