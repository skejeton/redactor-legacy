#ifndef R_REDEX_H
#define R_REDEX_H

#include <stdint.h>
#include <stdlib.h>
#include "src/Utf8.h"

enum {
    Redex_SubGroup_Char,
    Redex_SubGroup_Group,
}
typedef Redex_SubGroup_Type;

enum {
    Redex_Quantifier_None,     //
    Redex_Quantifier_All,      // *
    Redex_Quantifier_Greedy,   // +
    Redex_Quantifier_Lazy      // ?
}
typedef Redex_Quantifier;

struct Redex_Group {
    struct Redex_SubGroup *subgroups;
    size_t subgroups_len;
}
typedef Redex_Group;

struct Redex_SubGroup {
    Redex_Quantifier quantifier;
    Redex_SubGroup_Type type;
    union {
        uint32_t ch;
        struct Redex_Group group;
    };
}
typedef Redex_SubGroup;

typedef Redex_Group Redex_CompiledExpression;

Redex_CompiledExpression Redex_Compile(const char *redex);

#endif
