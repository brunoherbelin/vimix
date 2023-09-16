#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <string>
#include <list>
#include <deque>

class Playlist
{
public:
    Playlist();
    Playlist& operator = (const Playlist& b);

    // load / save XML
    void clear ();
    void load (const std::string &filename);
    void saveAs (const std::string &filename);
    bool save ();

    // add / remove / test by path
    bool add    (const std::string &path);
    size_t add  (const std::list<std::string> &list);
    void remove (const std::string &path);
    bool has    (const std::string &path) const;

    // access by index, from 0 to size()
    inline size_t size () const { return path_.size(); }
    inline std::string at (size_t index) const { return path_.at(index); }
    void remove (size_t index);
    void move   (size_t from_index, size_t to_index);
    inline size_t current() const { return current_index_; }

private:

    std::deque< std::string > path_;
    std::string filename_;
    size_t current_index_;
};

#endif // PLAYLIST_H
