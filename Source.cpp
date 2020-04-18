
#include <algorithm>

#include "Source.h"

// gobal static list of all sources
SourceList Source::sources_;


Source::Source(std::string name = "") : name_(""), buffer_(nullptr), shader_(nullptr), surface_(nullptr)
{
    // set a name
    rename(name);

    // add source to the list
    sources_.push_back(this);
}

Source::~Source()
{
    sources_.remove(this);
}

std::string Source::rename (std::string newname)
{
    // tentative new name
    std::string tentativename = newname;

    // refuse to rename to an empty name
    if ( newname.empty() )
        tentativename = "source";

    // trivial case : same name as current
    if ( tentativename == name_ )
        return name_;

    // search for a source of the name 'tentativename'
    std::string basename = tentativename;
    int count = 1;
    while ( std::find_if(sources_.begin(), sources_.end(), hasName(tentativename)) != sources_.end() ) {
        tentativename = basename + std::to_string(++count);
    }

    // ok to rename
    name_ = tentativename;
    return name_;
}


SourceList::iterator Source::begin()
{
    return sources_.begin();
}

SourceList::iterator Source::end()
{
    return sources_.end();
}

uint Source::numSource()
{
    return sources_.size();
}
