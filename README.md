# SxC

SxC is a C library that acts as a language binding target.  If, for example, you are a C library author and you want your library to be accessible from multiple scripting languages, simply write a wrapper for each function you want to expose and SxC will take care of exposing the functions in each available scripting language.

Alternatively, if you're writing a C library from scratch and you target SxC from the start, you can create and use lists and maps native to scripting language itself.  That is, instead of implementing your own dynamic array or hash map in C, populating it with data, then trying to push that data into the scripting environment, you can use SxC to instantiate a list or map already inside the scripting environment, then populate it directly.  You can also accept args that represent methods in the scripting environment and call out to them from your C function (to implement callbacks, for example).

SxC also provides facilities for type coercion (including string <-> numeric, and list <-> primitive array), exception-like errors in C (via internally managed longjmp), and on-the-stack memory allocation.  More features to come!
