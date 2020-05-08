#include <algorithm>

#include "defines.h"
#include "FrameBuffer.h"
#include "Session.h"
#include "GarbageVisitor.h"

Session::Session()
{

}
Session::~Session()
{
    // delete all sources
    for(auto it = sources_.begin(); it != sources_.end(); ) {
        // erase this source from the list
        it = deleteSource(*it);
    }

}

// update all sources
void Session::update(float dt)
{
    // pre-render of all sources
    for( SourceList::iterator it = sources_.begin(); it != sources_.end(); it++){
        (*it)->render( );
    }

    // update the scene tree
    render_.update(dt);

    // draw render view in Frame Buffer
    render_.draw();
}


SourceList::iterator Session::addSource(Source *s)
{
    // insert the source in the rendering
    render_.scene.root()->addChild(s->group(View::RENDERING));
    // insert the source to the beginning of the list
    sources_.push_front(s);
    // return the iterator to the source created at the beginning
    return sources_.begin();
}

SourceList::iterator Session::deleteSource(Source *s)
{
    // find the source
    SourceList::iterator its = find(s);
    // ok, its in the list !
    if (its != sources_.end()) {

        // remove Node from the rendering scene
//        GarbageVisitor remover(s->group(View::RENDERING));
//        remover.visit(render_.scene);

        render_.scene.root()->detatchChild( s->group(View::RENDERING) );

        // erase the source from the update list & get next element
        its = sources_.erase(its);

        // delete the source : safe now
        delete s;

        // NB: GarbageVisitor ends here, and deletes the Group RENDERING
    }

    // return end of next element
    return its;
}


SourceList::iterator Session::begin()
{
    return sources_.begin();
}

SourceList::iterator Session::end()
{
    return sources_.end();
}

SourceList::iterator Session::find(int index)
{
    if (index<0)
        return sources_.end();

    int i = 0;
    SourceList::iterator it = sources_.begin();
    while ( i < index && it != sources_.end() ){
        i++;
        it++;
    }
    return it;
}

SourceList::iterator Session::find(Source *s)
{
    return std::find(sources_.begin(), sources_.end(), s);
}

SourceList::iterator Session::find(std::string namesource)
{
    return std::find_if(sources_.begin(), sources_.end(), hasName(namesource));
}

SourceList::iterator Session::find(Node *node)
{
    return std::find_if(sources_.begin(), sources_.end(), hasNode(node));
}

uint Session::numSource() const
{
    return sources_.size();
}

int Session::index(SourceList::iterator it) const
{
    int index = -1;
    int count = 0;
    for(auto i = sources_.begin(); i != sources_.end(); i++, count++) {
        if ( i == it ) {
            index = count;
            break;
        }
    }
    return index;
}

