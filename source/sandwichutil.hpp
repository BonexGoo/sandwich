#pragma once
#include <boss.hpp>

class SandWichUtil
{
public:
    static String ToCRC64(bytes binary, sint32 length);
    static String ToBASE64(bytes binary, sint32 length);
    static buffer FromBASE64(chars base64, sint32 length);
};
