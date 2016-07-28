License
-------
egdb_intl Copyright (C) 2016 Ed Gilbert.
This software is distributed under the Boost Software License version 1.0.
See license.txt for more details.

Description
-----------
Egdb_intl is a set of C++ source files with functions to access the kingsrow international draughts endgame databases. The code can be used to access 2 versions of the win, loss, draw (WLD) db, and the moves-to-conversion (MTC) db. These databases have information for positions with up to 8 pieces and with up to 5 pieces on one side. The source files are identical to those used in the kingsrow draughts program.

From 2010 to 2014 I distributed version 1 of the databases on a portable external hard drive. This db is 407gb of data, which includes a small subset of 9-piece positions, 5 men vs. 4 men. In 2014 I created version 2 by re-compressing the db using better compression techniques, and excluding more positions to make the compression work better. Version 2 is 56gb of data, and is available for download from a file server. See link below.

The source code can be compiled for Windows using Microsoft Visual Studio 2015, and for Linux. Rein Halbersma ported the code to Linux and improved it in other ways also. Thank's Rein!

The function to lookup the value of a position in the database is thread-safe and can be used in a multi-threaded search engine.

You can contact me at eygilbert@gmail.com if you have questions or comments about these db drivers.


Obtaining the databases
-----------------------
The databases can be downloaded from the Mega file server using the links at this web page: 


Compiling
---------



Using the drivers
-----------------
To primary functions for using the databases are open, lookup, and close. The public interface to the databases are contained in egdb_intl.h. Include this file in any source file that needs to interact with the dbs.

To access a db for probing, it needs to be opened using the function egdb_open. Opening a db takes some time. It allocates memory for caching db values, and reads indexing files that allow the db to be probed quickly during an engine search. When you open a database you give it the path to the directory where the db files are located, and tell it how much memory it can use for caching and internal state data. You can optionally restrict the driver to access only a subset of the total positions in the db, by number of pieces, and by number of kings in 8-piece positions.

Values in the db can be queried by calling through the lookup function pointer returned from egdb_open. Note that in general the databases do not have a value for every position with N pieces. Some positions are excluded to make the compression more effective, so you need to be aware of which positions are excluded when probing. See more below in the section "Excluded positions".

An opened driver can also be queried for several attributes of the database, such as the maximum number of draughts pieces it has data for. Another function does a crc verification of all the db files.

When a user is finished with the driver, it can be closed to free up resources. The close function is accessed through another function pointer returned by the call to egdb_open.


Excluded Positions
------------------
Version 1 of the WLD db is identified as type EGDB_WLD_TUN_V1. Version 1 filenames have suffixes ".cpr" and ".idx". This db does not have valid data for any position that is a capture for the side-to-move. There is no way to know this from the lookup return value, which will typically be one of the win, loss, or draw values. Before calling lookup, you should first test that the position is not a capture.

Version 2 of the WLD db is identified as type EGDB_WLD_TUN_V2. Version 2 filenames have suffixes ".cpr1" and ".idx1". Like version 1, it does not have valid data for positions that are a capture for the side-to-move. Additionaly, for positions with 7 and 8 pieces that have more than 1 king present, it does not have valid data for positions that would be captures for the opposite of the side-to-move (non-side captures), and it only has data for one of the two side-to-move colors. Which side-to-move color has valid data varies with position, so you have to test the return value. If the value is EGDB_UNKNOWN, that side-to-move data for the position is not present in the db.

The MTC db is identified as type EGDB_MTC_RUNLEN. It has filenames with suffixes ".cpr_mtc" and ".idx_mtc". This db has data for both side-to-move colors, and for positions with non-side capture. To make the db considerably smaller, it does not have data for positions where the distance to a conversion move is less than 10 plies. The return from lookup for such positions is MTC_LESS_THAN_THRESHOLD. Draughts engines typically do not need databases for such positions because a heuristic search will easily provide a good move. The db only has valid data for positions that are a win or loss. You cannot tell this from the return value of a lookup in the MTC db, so before querying an MTC value you should use a WLD db to verify that the position is a win or a loss. 

