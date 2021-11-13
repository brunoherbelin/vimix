#ifndef METRONOME_H
#define METRONOME_H

#include <chrono>

class Metronome
{
    // Private Constructor
    Metronome();
    Metronome(Metronome const& copy) = delete;
    Metronome& operator=(Metronome const& copy) = delete;

public:

    static Metronome& manager ()
    {
        // The only instance
        static Metronome _instance;
        return _instance;
    }

    bool init ();
    void terminate ();

    double beats () const;
    double phase () const;

    void setTempo (double t);
    double tempo () const;

    void setQuantum (double q);
    double quantum () const;

    std::chrono::microseconds timeToBeat();

    size_t peers () const;
};

#endif // METRONOME_H
