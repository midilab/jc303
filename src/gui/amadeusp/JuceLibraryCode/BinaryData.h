/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   filter_png;
    const int            filter_pngSize = 76075;

    extern const char*   jc303gui_png;
    const int            jc303gui_pngSize = 433987;

    extern const char*   largerotary_png;
    const int            largerotary_pngSize = 6201;

    extern const char*   mediumrotary_png;
    const int            mediumrotary_pngSize = 6960;

    extern const char*   smallrotary_png;
    const int            smallrotary_pngSize = 2449;

    extern const char*   switch_png;
    const int            switch_pngSize = 7665;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 6;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
