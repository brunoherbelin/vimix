#include <algorithm>

#include "Selection.h"

Selection::Selection()
{

}


void Selection::add(Source *s)
{
    selection_.push_back(s);
    s->setMode(Source::ACTIVE);
}

void Selection::set(SourceList l)
{
    clear();

    for(auto it = l.begin(); it != l.end(); it++)
        (*it)->setMode(Source::ACTIVE);

    l.sort();
    l.unique();
    selection_ = l;
}

void Selection::add(SourceList l)
{
    for(auto it = l.begin(); it != l.end(); it++)
        (*it)->setMode(Source::ACTIVE);

    // generate new set as union of current selection and give list
    SourceList result;
    std::set_union(selection_.begin(), selection_.end(), l.begin(), l.end(), std::inserter(result, result.begin()) );

    // set new selection
    result.sort();
    result.unique();
    selection_ = SourceList(result);
}

void Selection::remove(Source *s)
{
    SourceList::iterator it = find(s);
    if (it != selection_.end()) {
        selection_.erase(it);
        s->setMode(Source::NORMAL);
    }
}

void Selection::remove(SourceList l)
{
    for(auto it = l.begin(); it != l.end(); it++)
        (*it)->setMode(Source::NORMAL);

    // generate new set as difference of current selection and give list
    SourceList result;
    std::set_difference(selection_.begin(), selection_.end(), l.begin(), l.end(), std::inserter(result, result.begin()) );
    // set new selection
    selection_ = SourceList(result);
}

void Selection::clear()
{
    for(auto it = selection_.begin(); it != selection_.end(); it++)
        (*it)->setMode(Source::NORMAL);

    selection_.clear();
}

uint Selection::size()
{
    return selection_.size();
}

SourceList::iterator Selection::find(Source *s)
{
    return std::find(selection_.begin(), selection_.end(), s);
}


bool Selection::contains (Source *s)
{
    return (find(s) != selection_.end());
}

SourceList::iterator Selection::begin()
{
    return selection_.begin();
}

SourceList::iterator Selection::end()
{
    return selection_.end();
}
