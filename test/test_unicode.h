#ifndef TEST_UNICODE_H
#define TEST_UNICODE_H

#include "report.h"
#include <forscape_unicode.h>

using namespace Forscape;

inline bool testUnicode(){
    bool passing = true;

    #define CONTINUATION_CHAR "\x82"
    #define TWO_BYTE_LEAD "\xC0"
    #define THREE_BYTE_LEAD "\xEE"
    #define FOUR_BYTE_LEAD "\xFF"
    #define ZERO_WIDTH_CODEPOINT "\xCC\x88"

    assert(!isIllFormedUtf8(""));
    assert(!isIllFormedUtf8("Hello world!"));
    assert(!isIllFormedUtf8("A = πr²"));
    assert(!isIllFormedUtf8("mẍ + cẋ + kx = F"));
    assert(!isIllFormedUtf8("\t\n\0"));
    assert(!isIllFormedUtf8("x" ZERO_WIDTH_CODEPOINT));
    assert(!isIllFormedUtf8(TWO_BYTE_LEAD CONTINUATION_CHAR));
    assert(!isIllFormedUtf8(THREE_BYTE_LEAD CONTINUATION_CHAR CONTINUATION_CHAR));
    assert(!isIllFormedUtf8(FOUR_BYTE_LEAD CONTINUATION_CHAR CONTINUATION_CHAR CONTINUATION_CHAR));
    assert(!isIllFormedUtf8("(╯°□°)╯︵ ┻━┻"));
    assert(!isIllFormedUtf8("( ͡° ͜ʖ ͡°)"));
    assert(!isIllFormedUtf8("Unicode is cool 😎"));
    assert(!isIllFormedUtf8("爱 кохання عشق Любовь אהבה 사랑 Αγάπη प्यार  Gràdh 愛する"));

    assert(isIllFormedUtf8("Continuation char without leading byte:" CONTINUATION_CHAR));
    assert(isIllFormedUtf8("Two continuation chars where one expected:" TWO_BYTE_LEAD CONTINUATION_CHAR CONTINUATION_CHAR));
    assert(isIllFormedUtf8("No continuation char with end:" TWO_BYTE_LEAD));
    assert(isIllFormedUtf8("No continuation char with text:" TWO_BYTE_LEAD "normal ASCII"));
    assert(isIllFormedUtf8(THREE_BYTE_LEAD CONTINUATION_CHAR));
    assert(isIllFormedUtf8(THREE_BYTE_LEAD CONTINUATION_CHAR CONTINUATION_CHAR CONTINUATION_CHAR));
    assert(isIllFormedUtf8(FOUR_BYTE_LEAD CONTINUATION_CHAR CONTINUATION_CHAR "normal ASCII"));
    assert(isIllFormedUtf8(ZERO_WIDTH_CODEPOINT "Leading zero-width codepoint"));

    report("UTF-8 handling", passing);
    return passing;
}


#endif // TEST_UNICODE_H
