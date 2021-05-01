#include "BaseToolkit.h"

#include <chrono>
#include <ctime>
#include <sstream>
#include <list>
#include <iomanip>
#include <algorithm>

#include <locale>
#include <unicode/ustream.h>
#include <unicode/translit.h>

uint64_t BaseToolkit::uniqueId()
{
    auto duration = std::chrono::high_resolution_clock::now().time_since_epoch();
    // 64-bit int                                                                  18446744073709551615UL
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000000000000UL;
}



std::string BaseToolkit::uniqueName(const std::string &basename, std::list<std::string> existingnames)
{
    std::string tentativename = basename;
    int count = 1;
    int max = 100;

    // while tentativename can be found in the list of existingnames
    while ( std::find( existingnames.begin(), existingnames.end(), tentativename ) != existingnames.end() )
    {
        for( auto it = existingnames.cbegin(); it != existingnames.cend(); ++it) {
            if ( it->find(tentativename) != std::string::npos)
                ++count;
        }

        if (count > 1)
            tentativename = basename + "_" + std::to_string( count );
        else
            tentativename += "_";

        if ( --max < 0 ) // for safety only, should never be needed
            break;
    }

    return tentativename;
}

// Using ICU transliteration :
// https://unicode-org.github.io/icu/userguide/transforms/general/#icu-transliterators

std::string BaseToolkit::transliterate(const std::string &input)
{
    auto ucs = icu::UnicodeString::fromUTF8(input);

    UErrorCode status = U_ZERO_ERROR;
    icu::Transliterator *firstTrans = icu::Transliterator::createInstance(
                "any-NFKD ; [:Nonspacing Mark:] Remove; NFKC; Latin", UTRANS_FORWARD, status);
    firstTrans->transliterate(ucs);
    delete firstTrans;

    icu::Transliterator *secondTrans = icu::Transliterator::createInstance(
                "any-NFKD ; [:Nonspacing Mark:] Remove; [@!#$*%~] Remove; NFKC", UTRANS_FORWARD, status);
    secondTrans->transliterate(ucs);
    delete secondTrans;

    std::ostringstream output;
    output << ucs;

    return output.str();
}


std::string BaseToolkit::byte_to_string(long b)
{
    double numbytes = static_cast<double>(b);
    std::ostringstream oss;

    std::list<std::string> list = {" Bytes", " KB", " MB", " GB", " TB"};
    std::list<std::string>::iterator i = list.begin();

    while(numbytes >= 1024.0 && i != list.end())
    {
        ++i;
        numbytes /= 1024.0;
    }
    oss << std::fixed << std::setprecision(2) << numbytes << *i;
    return oss.str();
}

std::string BaseToolkit::bits_to_string(long b)
{
    double numbytes = static_cast<double>(b);
    std::ostringstream oss;

    std::list<std::string> list = {" bit", " Kbit", " Mbit", " Gbit", " Tbit"};
    std::list<std::string>::iterator i = list.begin();

    while(numbytes >= 1000.0 && i != list.end())
    {
        ++i;
        numbytes /= 1000.0;
    }
    oss << std::fixed << std::setprecision(2) << numbytes << *i;
    return oss.str();
}


std::string BaseToolkit::trunc_string(const std::string& path, int N)
{
    std::string trunc = path;
    int l = path.size();
    if ( l > N ) {
        trunc = std::string("...") + path.substr( l - N + 3 );
    }
    return trunc;
}
