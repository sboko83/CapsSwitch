#ifndef CAPSSWITCH_VERSION_H
#define CAPSSWITCH_VERSION_H

/* The single source of the version: resources, About, package name. */
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0

#define VERSION_STR_(a, b, c) #a "." #b "." #c
#define VERSION_STR_X(a, b, c) VERSION_STR_(a, b, c)
#define VERSION_STRING VERSION_STR_X(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH)
#define VERSION_WIDE_(s) L##s
#define VERSION_WIDE_X(s) VERSION_WIDE_(s)
#define VERSION_WSTRING VERSION_WIDE_X(VERSION_STRING)

#endif
