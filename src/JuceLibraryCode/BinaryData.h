/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   jc303gui_png;
    const int            jc303gui_pngSize = 212567;

    extern const char*   jc303knob_png;
    const int            jc303knob_pngSize = 1380653;

    extern const char*   jc303modknob_png;
    const int            jc303modknob_pngSize = 49364;

    extern const char*   jc303modswitch_png;
    const int            jc303modswitch_pngSize = 4573;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 4;

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
