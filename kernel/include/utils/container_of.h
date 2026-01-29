#pragma once

#define container_of(PTR, TYPE, MEMBER) ({  \
    const typeof( ((TYPE *)0)->MEMBER ) *__mptr = (PTR);    \
    (TYPE *)( (uintptr_t)__mptr - __builtin_offsetof(TYPE, MEMBER) );})
