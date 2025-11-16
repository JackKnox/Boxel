#pragma once

#include "defines.h"

// A 2-element vector.
typedef union vec2 {
    f32 elements[2];
    struct {
        union {
            f32 x,
                r,
                s,
                u;
        };
        union {
            f32 y,
                g,
                t,
                v;
        };
    };
} vec2;

// A 3-element vector.
typedef union vec3 {
    f32 elements[3];
    struct {
        union {
            f32 x,
                r,
                s,
                u;
        };
        union {
            f32 y,
                g,
                t,
                v;
        };
        union {
            f32 z,
                b,
                p,
                w;
        };
    };
} vec3;

// A 4-element vector.
typedef union vec4 {
    f32 elements[4];
    union {
        struct {
            union {
                f32 x,
                    r,
                    s;
            };
            union {
                f32 y,
                    g,
                    t;
            };
            union {
                f32 z,
                    b,
                    p,
                    width;
            };
            union {
                f32 w,
                    a,
                    q,
                    height;
            };
        };
    };
} vec4;

// A unsigned integer 2-element vector.
typedef union uvec2 {
    u32 elements[2];
    struct {
        union {
            u32 x,
                r,
                s,
                u;
        };
        union {
            u32 y,
                g,
                t,
                v;
        };
    };
} uvec2;

// A unsigned interger 3-element vector.
typedef union uvec3 {
    u32 elements[3];
    struct {
        union {
            u32 x,
                r,
                s,
                u;
        };
        union {
            u32 y,
                g,
                t,
                v;
        };
        union {
            u32 z,
                b,
                p,
                w;
        };
    };
} uvec3;