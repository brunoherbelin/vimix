
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>

using namespace std;

#include <gst/gst.h>

#include "GstToolkit.h"

string GstToolkit::date_time_string()
{
    chrono::system_clock::time_point now = chrono::system_clock::now();
    time_t t = chrono::system_clock::to_time_t(now);
    std::tm* datetime = std::localtime(&t);

    auto duration = now.time_since_epoch();
    auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count() % 1000;

    ostringstream oss;
    oss << setw(4) << setfill('0') << std::to_string(datetime->tm_year + 1900);
    oss << setw(2) << setfill('0') << std::to_string(datetime->tm_mon + 1);
    oss << setw(2) << setfill('0') << std::to_string(datetime->tm_mday );
    oss << setw(2) << setfill('0') << std::to_string(datetime->tm_hour );
    oss << setw(2) << setfill('0') << std::to_string(datetime->tm_min );
    oss << setw(2) << setfill('0') << std::to_string(datetime->tm_sec );
    oss << setw(3) << setfill('0') << std::to_string(millis);

    // fixed length string (17 chars) YYYYMMDDHHmmssiii
    return oss.str();
}

string GstToolkit::time_to_string(guint64 t)
{
    if (t == GST_CLOCK_TIME_NONE)
        return "00:00:00.00";

    guint ms =  GST_TIME_AS_MSECONDS(t);
    guint s = ms / 1000;

    ostringstream oss;
    if (s / 3600)
        oss << setw(2) << setfill('0') << s / 3600 << ':';
    if ((s % 3600) / 60)
        oss << setw(2) << setfill('0') << (s % 3600) / 60 << ':';
    oss << setw(2) << setfill('0') << (s % 3600) % 60 << '.';
    oss << setw(2) << setfill('0') << (ms % 1000) / 10;

    // fixed length string (11 chars) HH:mm:ss.ii"
    return oss.str();
}


list<string> GstToolkit::all_plugins()
{
    list<string> pluginlist;
    GList *l, *g;

    l = gst_registry_get_plugin_list (gst_registry_get ());

    for (g = l; g; g = g->next) {
        GstPlugin *plugin = GST_PLUGIN (g->data);
        pluginlist.push_front( string( gst_plugin_get_name (plugin) ) );
    }

    gst_plugin_list_free (l);

    return pluginlist;
}


list<string> GstToolkit::all_plugin_features(string pluginname)
{
    list<string> featurelist;
    GList *l, *g;

    l = gst_registry_get_feature_list_by_plugin (gst_registry_get (), pluginname.c_str());

    for (g = l; g; g = g->next) {
        GstPluginFeature *feature = GST_PLUGIN_FEATURE (g->data);
        featurelist.push_front( string( gst_plugin_feature_get_name (feature) ) );
    }

    gst_plugin_feature_list_free (l);

    return featurelist;
}

bool GstToolkit::enable_feature (string name, bool enable) {
    GstRegistry *registry = NULL;
    GstElementFactory *factory = NULL;

    registry = gst_registry_get();
    if (!registry) return false;

    factory = gst_element_factory_find (name.c_str());
    if (!factory) return false;

    if (enable) {
        gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE (factory), GST_RANK_PRIMARY + 1);
    }
    else {
        gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE (factory), GST_RANK_NONE);
    }

    gst_registry_add_feature (registry, GST_PLUGIN_FEATURE (factory));
    return true;
}


string GstToolkit::gst_version()
{
    std::ostringstream oss;
    oss << GST_VERSION_MAJOR << '.' << GST_VERSION_MINOR << '.';
    oss << std::setw(2) << setfill('0') << GST_VERSION_MICRO ;
    if (GST_VERSION_NANO > 0)
        oss << ( (GST_VERSION_NANO < 2 ) ? " - (CVS)" : " - (Prerelease)");

    return oss.str();
}
