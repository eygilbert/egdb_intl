License
-------
egdb_intl Copyright (C) 2016 Ed Gilbert.
This software is distributed under the Boost Software License version 1.0.
See license.txt for more details.

Description
-----------
Egdb_intl is a set of C++ source files with functions to access the kingsrow international draughts endgame databases. The code can be used to access 2 versions of the win, loss, draw (WLD) db, and the moves-to-conversion (MTC) db. These databases have information for positions with up to 8 pieces and with up to 5 pieces on one side. The source files are identical to those used in the kingsrow draughts program.

From 2010 to 2014 I distributed version 1 of the databases on a portable external hard drive. This db is 407gb of data, which includes a small subset of 9-piece positions, 5 men vs. 4 men. In 2014 I created version 2 by re-compressing the db using better compression techniques, and excluding more positions to make the compression work better. Version 2 is 56gb of data, and is available for download from a file server. See link below.

The source code can be compiled for Windows using Microsoft Visual Studio 2015, and for Linux. Rein Halbersma ported the code to Linux and improved it in other ways also. Thanks Rein!

The function to lookup the value of a position in the database is thread-safe and can be used in a multi-threaded search engine.

You can contact me at eygilbert@gmail.com if you have questions or comments about these drivers.


Obtaining the databases
-----------------------
The databases can be downloaded from the Mega file server using the links at this web page: 


Compiling
---------



Using the drivers
-----------------
The primary functions for using the databases are open, lookup, and close. The public interface to the databases are contained in egdb_intl.h. Include this file in any source file that needs to interface with the dbs.

To access a db for probing, it needs to be opened using the function egdb_open. This returns a driver handle that contains function pointers for subsequent queries into the db.

Values in the db can be queried by calling through the lookup function pointer in the driver handle. Note that in general the databases do not have a value for every position with N pieces. Some positions are excluded to make the compression more effective, and you cannot always tell this from the value returned by lookup, so you need to be aware of which positions are excluded when probing. See more below in the section "Excluded positions".

An opened driver can also be queried for several attributes of the database, such as the maximum number of draughts pieces it has data for. Another function does a crc verification of all the db files.

When a user is finished with the driver, it can be closed to free up resources. The close function is accessed through another function pointer in the driver handle.


Excluded Positions
------------------
Version 1 of the WLD db is identified as type EGDB_WLD_TUN_V1. Version 1 filenames have suffixes ".cpr" and ".idx". This db does not have valid data for any position that is a capture for the side-to-move. There is no way to know this from the lookup return value, which will typically be one of the win, loss, or draw values. Before calling lookup, you should first test that the position is not a capture.

Version 2 of the WLD db is identified as type EGDB_WLD_TUN_V2. Version 2 filenames have suffixes ".cpr1" and ".idx1". Like version 1, it does not have valid data for positions that are a capture for the side-to-move. Additionally, for positions with 7 and 8 pieces that have more than 1 king present, it does not have valid data for positions that would be captures for the opposite of the side-to-move (non-side captures), and it only has data for one side. Which side is excluded varies with position. It's not always black or always white, although it is consistent within each slice. It's whatever makes the compression more effective. If you call lookup and the data for the side that is not present, you get the return EGDB_UNKNOWN. 

The MTC db is identified as type EGDB_MTC_RUNLEN. It has filenames with suffixes ".cpr_mtc" and ".idx_mtc". This db has data for both side-to-move colors, and for positions with non-side capture. To make the db considerably smaller, it does not have data for positions where the distance to a conversion move is less than 10 plies. The return from lookup for such positions is MTC_LESS_THAN_THRESHOLD. Draughts engines typically do not need databases for such positions because a search can usually provide a good move. The db only has valid data for positions that are a win or loss. You cannot tell this from the return value of a lookup in the MTC db, so before querying an MTC value you should use a WLD db to determine that the position is a win or a loss. 

Functions
---------
EGDB_DRIVER *egdb_open(char const *options, int cache_mb, char const *directory, void (*msg_fn)(char const *msg));

Opens a database. The return value is a pointer to a structure that has function pointers for subsequent communication with the driver. A NULL return value means that an error occurred. 

Opening a db takes some time. It allocates memory for caching db values, and reads indexing files that allow the db to be probed quickly during an engine search.

"options" is a string of optional open settings. The options are of the form name=value. Multiple options are separated by a semicolon. Either a NULL pointer or an empty string ("") can be given for no options. The options were more useful a few years ago when computers had less memory. Now they are not usually needed. These options are defined:

"maxpieces" sets the maximum number of pieces for which the driver will lookup values. By default the driver will use all the database files that it finds during initialization. If you restrict the number of pieces, the driver will not have to load indexing data for larger position, saving time during initialization and using it allotted memory more efficiently.

"maxkings_1side_8pcs" restricts the set of 8-piece positions for which the driver will lookup values. This saves time and memory during initialization.

"cache_mb" is the number of megabytes (2^20 bytes) of heap memory that the driver will use. Some of it is used for indexing data, and the rest is used to dynamically cache database data to minimize disk access during lookups. The more memory you give the driver, the faster it works on average during an engine search. On a machine with at least 8gb of ram, setting the cache_mb to something like 3000mb less than the total PC memory gives good driver performance and still leaves some memory for Windows drivers and a few other smaller programs to run. On machines with less memory you'll have to make the margin smaller and manage the memory usage more carefully. My experience is that the driver actually works surprisingly well in a kingsrow search with the 8pc db and a very small setting like 1500mb, but of course more memory is better.

"directory" is the path to the location of the database files.

"msg_fn" is a pointer to a function that will receive status and error messages from the driver. The driver normally emits status messages when it starts up, with information about files opened and memory used. If any errors occur, either during the open call or in subsequent calls through the driver handle, the details will be reported through the message function. A message function that writes messages to stdout can be defined like this:

        void msg_fn(char const *msg)
        {
             printf(msg);
        }

