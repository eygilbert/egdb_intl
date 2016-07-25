License
-------
egdb_intl Copyright (C) 2016 Ed Gilbert.
This software is distributed under the Boost Software License version 1.0.
See license.txt for more details.

Description
-----------
Egdb_intl is a set of functions that can be used to access the kingsrow international draughts endgame databases. The functions are provided in C++ source code form. Makefiles are provided for both Microsoft Visual Studio 2015, and Linux. The code can be used to access the most recent version of the win, loss, draw (WLD) db, and the moves-to-conversion (MTC) db. 

Most of the functions are thread-safe and can be used in a multi-threaded search engine.


Obtaining the databases
-----------------------
The databases can be downloaded from the Mega file server using the links at this web page: 


Compiling
---------



Using the drivers
-----------------
To access a db for probing, it needs to be opened using the function egdb_open. Opening a db takes some time. It allocates memory for caching db values, and reads some indexing files that allow the db to be read quickly for use during an engine search. When you open a database you give it the path to the directory where the db files are located, and tell it how much memory it can use for caching and internal state data. You can optionally restrict the driver to access only a subset of the total positions in the db, by number of pieces, and by number of kings in 8-piece positions.

Values in the db can be queried by calling through one of the function pointers returned from egdb_open. Note that in general the databases do not have a value for every position with N pieces. Some positions are excluded to make the compression more effective, so you need to be aware of which positions are excluded when probing. See more below in the section "Excluded positions".

An opened driver can also be queried for several attributes of the database, such as the maximum number of draughts pieces it has data for. Another function does a crc verification of all the db files.

When a user is finished with the driver, it can be closed to free up resources. The close function is accessed through another function pointer returned by the call to egdb_open.


Excluded Positions
------------------


