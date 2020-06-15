#ifndef SELECTION_H
#define SELECTION_H

#include "Source.h"

class Selection
{

public:
    Selection();

    void add    (Source *s);
    void add    (SourceList l);
    void remove (Source *s);
    void remove (SourceList l);

    void set    (SourceList l);
    void clear  ();

    SourceList::iterator begin ();
    SourceList::iterator end ();
    bool contains (Source *s);

protected:
    SourceList::iterator find (Source *s);
    SourceList selection_;

};

#endif // SELECTION_H
