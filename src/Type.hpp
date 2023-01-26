#pragma once

#include <cassert>

// TODO: Use other representation to handle struct
enum Type
{
    typeUnknown,
    typeVoid,
    typeInt,
    typeChar
};

inline const char* ToString( Type type )
{
    switch( type )
    {
        case typeUnknown:
            return "<unknown>";
        case typeVoid:
            return "void";
        case typeInt:
            return "int";
        case typeChar:
            return "char";
        default:
            return "";
    }
}