#include <algorithm>

#include "defines.h"
#include "SessionVisitor.h"
#include "Source.h"

#include "Selection.h"

Selection::Selection()
{
}


void Selection::add(Source *s)
{
    if (s == nullptr)
        return;

    selection_.push_back(s);
    selection_.sort();
    selection_.unique();
    s->setMode(Source::SELECTED);
}

void Selection::remove(Source *s)
{
    if (s == nullptr)
        return;

    SourceList::iterator it = find(s);
    if (it != selection_.end()) {
        selection_.erase(it);
        s->setMode(Source::VISIBLE);
    }
}

void Selection::toggle(Source *s)
{
    if (s == nullptr)
        return;

    if (contains(s))
        remove(s);
    else
        add(s);
}

void Selection::set(Source *s)
{
    clear();

    if (s == nullptr)
        return;

    selection_.push_back(s);
    s->setMode(Source::SELECTED);
}


void Selection::set(SourceList l)
{
    clear();

    for(auto it = l.begin(); it != l.end(); ++it)
        (*it)->setMode(Source::SELECTED);

    l.sort();
    l.unique();
    selection_ = l;
}

void Selection::add(SourceList l)
{
    for(auto it = l.begin(); it != l.end(); ++it)
        (*it)->setMode(Source::SELECTED);

    // generate new set as union of current selection and give list
    SourceList result;
    std::set_union(selection_.begin(), selection_.end(), l.begin(), l.end(), std::inserter(result, result.begin()) );

    // set new selection
    result.sort();
    result.unique();
    selection_ = SourceList(result);
}

void Selection::remove(SourceList l)
{
    for(auto it = l.begin(); it != l.end(); ++it)
        (*it)->setMode(Source::VISIBLE);

    // generate new set as difference of current selection and give list
    SourceList result;
    std::set_difference(selection_.begin(), selection_.end(), l.begin(), l.end(), std::inserter(result, result.begin()) );
    // set new selection
    selection_ = SourceList(result);
}

void Selection::clear()
{
    for(auto it = selection_.begin(); it != selection_.end(); ++it)
        (*it)->setMode(Source::VISIBLE);

    selection_.clear();
}

uint Selection::size() const
{
    return selection_.size();
}

Source *Selection::front()
{
    if (selection_.empty())
        return nullptr;

    return selection_.front();
}

Source *Selection::back()
{
    if (selection_.empty())
        return nullptr;

    return selection_.back();
}

void Selection::pop_front()
{
    if (!selection_.empty()) // TODO set mode ?
        selection_.pop_front();
}

bool Selection::empty() const
{
    return selection_.empty();
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

std::string Selection::clipboard() const
{
    return SessionVisitor::getClipboard(selection_);
}

SourceList Selection::getCopy() const
{
    SourceList dsl = selection_;
    return dsl;
}

