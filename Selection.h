#ifndef SELECTION_H
#define SELECTION_H

#include "Source.h"

class Selection
{

public:
    Selection();

    void add    (Source *s);
    void remove (Source *s);
    void set    (Source *s);
    void toggle (Source *s);

    void add    (SourceList l);
    void remove (SourceList l);
    void set    (SourceList l);
    void clear  ();

    SourceList::iterator begin ();
    SourceList::iterator end ();
    Source *front();
    bool contains (Source *s);
    bool empty();
    uint size ();

    std::string xml();

protected:
    SourceList::iterator find (Source *s);
    SourceList selection_;
};

#endif // SELECTION_H
