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
                u,
                width;
        };
        union {
            f32 y,
                g,
                t,
                v,
                height;
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
                u,
                width;
        };
        union {
            f32 y,
                g,
                t,
                v,
                height;
        };
        union {
            f32 z,
                b,
                p,
                w,
                length;
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
                    p;
            };
            union {
                f32 w,
                    a,
                    q;
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
                u,
                width;
        };
        union {
            u32 y,
                g,
                t,
                v,
                height;
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
                u,
                width;
        };
        union {
            u32 y,
                g,
                t,
                v,
                height;
        };
        union {
            u32 z,
                b,
                p,
                w,
                length;
        };
    };
} uvec3;