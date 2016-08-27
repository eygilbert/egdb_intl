# `egdb_intl`: a driver for international draughts endgame databases

`egdb_intl` is a set of C++ source files with functions to access the Kingsrow international draughts endgame databases. This package has been designed so that it can be easily integrated into existing C++ draughts programs. The source files are identical to those in the Kingsrow draughts program. 

The source code can be compiled for Windows using Microsoft Visual Studio 2015, and for Linux. 

The function to lookup the value of a position in the database is thread-safe and can be used in a multi-threaded search engine.

The code can be used to access 2 versions of the win, loss, draw (WLD) db, and the moves-to-conversion (MTC) db. These databases have information for positions with up to 8 pieces and with up to 5 pieces on one side.

From 2010 to 2014 I distributed version 1 of the databases on a portable external hard drive. This db is 407gb of data, which includes a small subset of 9-piece positions, 5 men vs. 4 men (I disabled the 9-piece functionality in this driver because I found through testing that on average it was less effective than not using it). In 2014 I created version 2 by re-compressing the db using better compression techniques, and excluding more positions to make the compression work better. Version 2 is 56gb of data, and is available for download from a file server. See link below.

You can contact me at eygilbert@gmail.com if you have questions or comments about these drivers.


# Obtaining the databases

The databases can be downloaded using the links at this web page: http://edgilbert.org/InternationalDraughts/endgame_database_downloads.htm

# Using the drivers

The primary functions for using the databases are `egdb_open()`, `egdb_lookup()`, and `egdb_close()`. The public interface to the databases are defined in `egdb_intl.h`. Include this file in any source file that needs to interface with the dbs. 

To avoid name collisions with user's public names, all declarations in `egdb_intl.h` are wrapped in a namespace named `egdb_interface`. You'll need to either include a `using namespace egdb_interface;` statement in your source files, or prefix any use of the driver interface names with the namespace name, e.g. `egdb_interface::egdb_open`. If you find the long namespace name annoying to type, you can define a short alias using something like 

    namespace dbi = egdb_interface; 
    
 and then use the alias instead of the full name.

To access a db for probing, it needs to be opened using the function `egdb_open()`. This returns a driver handle that contains function pointers for subsequent queries into the db.

Values in the db can be queried by calling through the `egdb_lookup()` function that takes a driver handle. Note that in general the databases do not have a value for every position with N pieces. Some positions are excluded to make the compression more effective, and you cannot always tell this from the value returned by `egdb_lookup()`, so you need to be aware of which positions are excluded and test for them before probing. See more below in the section "Excluded positions".

An opened driver can also be queried for several attributes of the database, such as the maximum number of draughts pieces it has data for. Another function does a crc verification of all the db files.

When a user is finished with the driver, it can be closed to free up resources. The `egdb_close()` function is accessed through another function pointer in the driver handle.

# Example program

The driver sources include an example program in the directory `example`. It opens a database, reads some positions from a table, calls `egdb_lookup()` to get the database value of each position, and compares it to the known value that is also in the table. You can use this to verify that basic functions of the driver are working on your system.

The example includes a project file for Microsoft Visual Studio 2015 (for which there is a free Community Edition), and a CMake script that will generate a Linux Makefile. To run the example, edit the macro `DB_PATH` near the top of `example/main.cpp` to point it to the location of your db files, then compile and run without arguments. On Windows, this typically is `C:/Program Files/Kingsrow International/wld_database` or `C:/Program Files/Kingsrow International/mtc_database`. On dual-boot or virtual machine Linux installations, one can mount these paths under `/media/` followed by a `sudo usermod -a -G vboxsf USERNAME`

To compile on Linux, do the usual CMake incantations for an out-of-tree build:

    cd egdbl_intl
    mkdir build
    cd build
    cmake ..
    make

When the build finishes, you can run `example.main` from the `build` directory by calling

    ./example/example.main

# Excluded positions

Version 1 of the WLD db is identified as type EGDB_WLD_TUN_V1. Version 1 filenames have suffixes ".cpr" and ".idx". This db does not have valid data for any position that is a capture for the side-to-move. There is no way to know this from the lookup return value. It will be a win, loss, or draw value, but it will not be the correct value except by random luck. It will be whatever makes the compression work best. Before calling lookup, you should first test that the position is not a capture.

Version 2 of the WLD db is identified as type EGDB_WLD_TUN_V2. Version 2 filenames have suffixes ".cpr1" and ".idx1". Like version 1, it does not have valid data for positions that are a capture for the side-to-move. Additionally, for positions with 7 and 8 pieces that have more than 1 king present, it does not have valid data for positions that would be captures for the opposite of the side-to-move (non-side captures), and it only has data for one side. Which side is excluded varies with position. It's not always black or always white, although it is consistent within each slice. It's whatever makes the compression more effective. If you call lookup and the data for that side is not present, you get the return EGDB_UNKNOWN. If you call lookup for a position that is excluded because of a capture or non-side capture, you will get a WLD value, but it will not be the correct value except by random luck. The value will be whatever makes the compression work best.

The MTC db is identified as type EGDB_MTC_RUNLEN. It has filenames with suffixes ".cpr_mtc" and ".idx_mtc". This db has data for both side-to-move colors, and for positions with non-side captures. To make the db considerably smaller, it does not have data for positions where the distance to a conversion move is less than 10 plies. The return from lookup for such positions is MTC_LESS_THAN_THRESHOLD. Draughts engines typically do not need databases for such positions because a search can usually provide a good move. The db only has valid data for positions that are a win or loss. You cannot tell this from the return value of a lookup in the MTC db, so before querying an MTC value you should use a WLD db to determine that the position is a win or a loss. 

# Reference documentation

The [reference documentation](doc/reference.md) of the entire `egdb/egdb_intl.h` header.

# Acknowledgements

Rein Halbersma ported the code to Linux and improved it in other ways also. Thanks Rein!

# License

    Original work Copyright (C) 2007-2016 Ed Gilbert
    Linux port Copyright (C) 2016 Rein Halbersma and Ed Gilbert

Distributed under the Boost Software License, Version 1.0.
(See accompanying file `LICENSE_1_0.txt` or copy at
[http://www.boost.org/LICENSE_1_0.txt](http://www.boost.org/LICENSE_1_0.txt))
