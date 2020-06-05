# `egdb_intl`: a driver for international draughts endgame databases

`egdb_intl` is a set of C++ source files with functions to access the Kingsrow international draughts endgame databases. This package can easily be integrated into existing C++ draughts programs. The source files are identical to those used in the Kingsrow draughts program.

For Java users, Jan-Jaap van Horssen has created a Java Native Interface to the drivers. A zip file containing the binaries and sources can be downloaded here: http://edgilbert.org/InternationalDraughts/downloads/KingsrowEgdbJava.zip

For other languages, see the repository branch named `dll`. There is a DLL interface to the drivers that can be used in many Windows environments. There are also several files that can be included in a VBA project in the folder VBA_test.

# Overview 

The source code can be compiled for Windows using Microsoft Visual Studio 2019, and for Linux using g++ or Clang. The function `egdb_lookup()` to lookup the value of a position in the database is thread-safe and can be used in a multi-threaded search engine.

The code can be used to access 2 versions of the win, loss, draw (WLD) database, a distance-to-win (DTW) database, and a distance-to-conversion (MTC) database. With the exception of the DTW db, these databases have information for positions with up to 8 pieces and with up to 5 pieces on one side. The DTW db has information for position with up to 7 pieces.

From 2010 to 2014 I distributed version 1 of the WLD databases on a portable external hard drive. This database is 407gb of data, which includes a small subset of 9-piece positions, 5 men vs. 4 men (I disabled the 9-piece functionality in this driver because I found through testing that on average it was less effective than not using it). In 2014, I created version 2 by re-compressing the database using better compression techniques, and excluding more positions to make the compression work better. Version 2 is 56gb of data, and is available for download from a file server. See link below.

You can contact me at [eygilbert@gmail.com](eygilbert@gmail.com) if you have questions or comments about these drivers.

# Installation

**Obtaining the databases**

The databases are standalone and do not depend on the Kingsrow draughts program. They can be downloaded using the links at this web page: [http://edgilbert.org/InternationalDraughts/endgame_database_downloads.htm](http://edgilbert.org/InternationalDraughts/endgame_database_downloads.htm). These databases are the same as the ones used in the Kingsrow draughts program. They can be used directly on both Windows or Linux systems.

**Re-using the databases of an existing Kingsrow installation**

Because the driver does not modify any of the databases, it is possible to re-use the databases of an existing installation of the Kingsrow draughts program.

  - On Windows: simply use the paths such as `C:/Program Files/Kingsrow International/wld_database`, `C:/Program Files/Kingsrow International/mtc_database`, or `C:/Program Files/Kingsrow International/dtw_database` when opening the databases. 
  - On Linux: for dual-boot or virtual machine Linux installations this typically involves mounting the filepath to e.g. `/media` and adding your username to the group with access to the mounted media (such as `vboxsf` for the [VirtualBox](https://www.virtualbox.org/wiki/Downloads) virtual machines). Consult your own particular Linux system reference on how to access Windows files on such hybrid setups (it might require installation of support packages such as VirtualBox guest additions). 

**Obtaining the driver**

The source code of this driver can be downloaded or cloned from GitHub

    git clone https://github.com/eygilbert/egdb_intl.git 
    
**System requirements**

The minimum requirements depend on the operating system in use (Windows or Linux), whether the system is 32-bit or 64-bit, and whether it is used in a single-threaded or a multi-threaded program.
  - On Windows: the Microsoft Visual C++ 2019 (for which there is a free [Community Edition](https://www.visualstudio.com/)) and higher. It is possible that earlier versions of Microsoft Visual C++ are also compatible. This is not supported however.
    - For 32-bit systems, the macro `USE_WIN_API` must have been defined.
    - For 64-bit systems, the macro `USE_WIN_API` can be used to switch between the Windows API and the C++ Standard Library.
  - On Linux: a 64-bit system and
    - For single-threaded programs: a C++98 conforming compiler (all known versions of g++ and Clang).
    - For multi-threaded programs: a C++11 conforming compiler with a Standard Library that supports the headers `<atomic>` and `<thread>`. The driver has been tested with g++ 4.8 and higher, and Clang 3.3 and higher.

**Compiling the driver**

  - On Windows: The `example` directory includes a project file for Microsoft Visual Studio 2019. Alternatively, one can use the CMake GUI for Windows to automatically generate a Visual Studio project file. To run the example, edit the macro `DB_PATH` near the top of `example/main.cpp` to point it to the location of your db files, then compile and run without arguments. When integrating the driver into your own Visual Studio projects, be aware that the driver uses C standard library functions that VS2019 will give warnings about. A simple way to turn off these warnings is by adding the preprocessor definition _CRT_SECURE_NO_WARNINGS to the project file. 
  - On Linux: do the usual CMake incantations for an out-of-tree build:


    mkdir build
    cd build
    cmake ..
    make

# Using the drivers

The public interface to the databases is defined in the header `egdb/egdb_intl.h`. Include this file in any source file that needs to interface with the databases, and also build and link against the various source files in the directories `builddb`, `egdb` and `engine`. The primary functions for using the databases are `egdb_open()`, `egdb_close()` and `egdb_lookup()`. 

To avoid name collisions with users' public names, all declarations in `egdb_intl.h` are wrapped in a namespace named `egdb_interface`. You'll need to either include a `using namespace egdb_interface;` statement in your source files, or prefix any use of the driver interface names with the namespace name, e.g. `egdb_interface::egdb_open`. If you find the long namespace name annoying to type, you can define a short alias using something like 

    namespace dbi = egdb_interface; 
    
 and then use the alias instead of the full name.

To access a database for probing, it needs to be opened using the function `egdb_open()`. This returns a driver handle that can be passed to `egdb_lookup()` and other functions. When a user is finished with the driver, it can be closed to free up resources using `egdb_close()`.

# Quick start

The example program below (which is a condened version of `example/main.cpp`) opens a database, reads some positions from a table, calls `egdb_lookup()` to get the database value of each position, and compares it to the known value that is also in the table. You can use this to verify that basic functions of the driver are working on your system.


    #include "egdb/egdb_intl.h"
    #include <cstdio>
    
    using namespace egdb_interface;
    // definitions of DB_PATH, positions, num_positions, print_msgs omitted for brevity
    
    int main()
    {
        // open the database, use at most 6 piece database, 1 GiB of memory cache
        EGDB_DRIVER *handle = egdb_open("maxpieces=6", 1024, DB_PATH, print_msgs);
        if (!handle) {
            std::printf("Cannot open egdb driver at %s\n", DB_PATH);
            return(1);
        }
    
        // read positions from table and compare looked up database values with the known values
        int nerrors = 0;
        std::printf("Testing %zd positions.\n", num_positions);
        for (int i = 0; i < num_positions; ++i) {
            int value = egdb_lookup(handle, &positions[i].pos, positions[i].color, 0);
            if (value != positions[i].value) {
                std::printf("%d: lookup error, expected %d, got %d\n", i, value, positions[i].value);
                ++nerrors;
            }
        }
        std::printf("Test complete, %d errors.\n", nerrors);
    
        // close the database, release all associated resources
        egdb_close(handle);
    }

---

# Handling excluded capture positions during `egdb_lookup()`

Version 1 of the WLD database is identified as type `EGDB_WLD_TUN_V1`. Version 1 filenames have suffixes ".cpr" and ".idx". This database does not have valid data for any position that is a capture for the side-to-move. There is no way to know this from the lookup return value. It will be a win, loss, or draw value, but it will not be the correct value except by random luck. It will be whatever makes the compression work best. 

**Guideline**: before calling `egdb_lookup()` on a version 1 WLD database, you should first test that the position is not a capture.

--- 

Version 2 of the WLD database is identified as type `EGDB_WLD_TUN_V2`. Version 2 filenames have suffixes ".cpr1" and ".idx1". Like version 1, it does not have valid data for positions that are a capture for the side-to-move. Additionally, for positions with 7 and 8 pieces that have more than 1 king present, it does not have valid data for positions that would be captures for the opposite of the side-to-move (non-side captures), and it only has data for one side. Which side is excluded varies with position. It's not always black or always white, although it is consistent within each slice. It's whatever makes the compression more effective. If you call `egdb_lookup()` and the data for that side is not present, you get the return value `EGDB_UNKNOWN`. If you call `egdb_lookup()` for a position that is excluded because of a capture or non-side capture, you will get a WLD value, but it will not be the correct value except by random luck. The value will be whatever makes the compression work best.

**Guideline**: before calling `egdb_lookup()` on a version 2 WLD database, you should first test that the position is not a capture, and for 7 and 8-piece positions with more than one king present, test that it is not a non-side capture.

---

The MTC database is identified as type `EGDB_MTC_RUNLEN`. It has filenames with suffixes ".cpr_mtc" and ".idx_mtc". This database has data for both side-to-move colors, and for positions with non-side captures. To make the database considerably smaller, it does not have data for positions where the distance to a conversion move is less than 10 plies. The return from `egdb_lookup()` for such positions is `MTC_LESS_THAN_THRESHOLD`. Draughts engines typically do not need databases for such positions because a search can usually provide a good move. The database only has valid data for positions that are a win or loss. You cannot tell this from the return value of a lookup in the MTC database.

**Guideline**: before querying an MTC value you should first use a WLD database to determine that the position is a win or a loss. 

---
The WLD database is identified as type `EGDB_DTW`. It has filenames with suffixes ".cpr_dtw" and ".idx_dtw". This database has data for positions with up to 7 pieces. It does not have data for positions that are draws, or positions that are captures. The return value from `egdb_lookup()` is the (distance to win or loss) / 2. To get the actual distances, multiply the return value by 2, and if the position is a win, add 1.

---

# Reference documentation

The [reference documentation](doc/reference.md) of the entire `egdb/egdb_intl.h` header.

# Acknowledgements

Rein Halbersma ported the code to Linux and improved it in other ways also. Thanks Rein!

# License

    Original work Copyright (C) 2007-2020 Ed Gilbert
    Linux port Copyright (C) 2016 Rein Halbersma and Ed Gilbert

Distributed under the Boost Software License, Version 1.0.
(See accompanying file [LICENSE_1_0.txt](LICENSE_1_0.txt) or copy at
[http://www.boost.org/LICENSE_1_0.txt](http://www.boost.org/LICENSE_1_0.txt))
