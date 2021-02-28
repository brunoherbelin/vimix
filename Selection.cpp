#include <algorithm>

#include "defines.h"
#include "SessionVisitor.h"
#include <tinyxml2.h>

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

    for(auto it = l.begin(); it != l.end(); it++)
        (*it)->setMode(Source::SELECTED);

    l.sort();
    l.unique();
    selection_ = l;
}

void Selection::add(SourceList l)
{
    for(auto it = l.begin(); it != l.end(); it++)
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
    for(auto it = l.begin(); it != l.end(); it++)
        (*it)->setMode(Source::VISIBLE);

    // generate new set as difference of current selection and give list
    SourceList result;
    std::set_difference(selection_.begin(), selection_.end(), l.begin(), l.end(), std::inserter(result, result.begin()) );
    // set new selection
    selection_ = SourceList(result);
}

void Selection::clear()
{
    for(auto it = selection_.begin(); it != selection_.end(); it++)
        (*it)->setMode(Source::VISIBLE);

    selection_.clear();
}

uint Selection::size()
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

bool Selection::empty()
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

std::string Selection::xml()
{
    std::string x = "";

    if (!selection_.empty()) {

        // create xml doc and root node
        tinyxml2::XMLDocument xmlDoc;
        tinyxml2::XMLElement *selectionNode = xmlDoc.NewElement(APP_NAME);
        selectionNode->SetAttribute("size", (int) selection_.size());
        xmlDoc.InsertEndChild(selectionNode);

        // fill doc
        SourceList selection_clones_;
        SessionVisitor sv(&xmlDoc, selectionNode);
        for (auto iter = selection_.begin(); iter != selection_.end(); iter++, sv.setRoot(selectionNode) ){
            // keep the clones for later
            CloneSource *clone = dynamic_cast<CloneSource *>(*iter);
            if (clone)
                (*iter)->accept(sv);
            else
                selection_clones_.push_back(*iter);
        }
        // add the clones at the end
        for (auto iter = selection_clones_.begin(); iter != selection_clones_.end(); iter++, sv.setRoot(selectionNode) ){

            (*iter)->accept(sv);
        }

        // get compact string
        tinyxml2::XMLPrinter xmlPrint(0, true);
        xmlDoc.Print( &xmlPrint );
        x = xmlPrint.CStr();
    }

    return x;
}

bool compare_depth (Source * first, Source * second)
{
  return ( first->depth() < second->depth() );
}

SourceList Selection::depthSortedList()
{
    SourceList dsl = selection_;
    dsl.sort(compare_depth);

    return dsl;
}

