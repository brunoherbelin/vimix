#ifndef METRONOME_H
#define METRONOME_H


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
    void terminate();


};

#endif // METRONOME_H
