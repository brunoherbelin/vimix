#ifndef SELECTION_H
#define SELECTION_H

#include <string>
#include "SourceList.h"

class Selection
{

public:
    Selection();

    // construct list
    void add    (Source *s);
    void add    (SourceList l);
    void remove (Source *s);
    void remove (SourceList l);
    void set    (Source *s);
    void set    (SourceList l);
    void toggle (Source *s);

    void clear  ();
    void pop_front();

    // access elements
    SourceList::iterator begin ();
    SourceList::iterator end ();
    Source *front();
    Source *back();

    // properties
    bool contains (Source *s);
    bool empty() const;
    uint size () const;

    // extract
    std::string clipboard() const;
    SourceList getCopy() const;

protected:
    SourceList::iterator find (Source *s);
    SourceList selection_;
};

#endif // SELECTION_H
