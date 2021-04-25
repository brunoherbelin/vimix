#include "BaseToolkit.h"

#include <chrono>
#include <ctime>
#include <sstream>
#include <list>
#include <iomanip>

#include <locale>
#include <unicode/ustream.h>
#include <unicode/translit.h>

uint64_t BaseToolkit::uniqueId()
{
    auto duration = std::chrono::high_resolution_clock::now().time_since_epoch();
    // 64-bit int                                                                  18446744073709551615UL
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000000000000UL;
}


// Using ICU transliteration :
// https://unicode-org.github.io/icu/userguide/transforms/general/#icu-transliterators

std::string BaseToolkit::transliterate(std::string input)
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
