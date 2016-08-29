# Reference documentation for `egdb_intl`

## Core types

### `egdb_interface::EGDB_DRIVER`
    struct EGDB_DRIVER {
        // implementation defined
    };
    
**Notes**: This is an opaque class whose interface consists entirely of global functions such as `egdb_open()`, `egdb_close()` and `egdb_lookup()` documented below. 

**Deprecated access**: in older versions of this driver, `EGDB_DRIVER*` variables named `handle` could be accessed through calls of the form `handle->some_function(handle, other_args)`. Such access is deprecated, however, and support for it can be removed in a future release. Instead, use the form `egdb_some_function(handle, other_args)`.

---

### `egdb_interface::EGDB_BITBOARD`
    #include <stdint.h>    
    typedef uint64_t EGDB_BITBOARD;
    
**Notes**: For 10x10 board representations, bits 0 through 53 represent squares 1 through 50, with gaps (sometimes called "ghost squares") at bits 10, 21, 32, and 43, and bits 54 through 63. See also [this thread](http://laatste.info/bb3/viewtopic.php?f=53&t=1925&start=60) on the FMJD forum.   

---

### `egdb_interface::EGDB_POSITION`    
    struct EGDB_POSITION {
        EGDB_BITBOARD black;
        EGDB_BITBOARD white;
        EGDB_BITBOARD king;
    };
    
**Notes**: a bitboard representation of an international draughts position containing the black pieces, white pieces and kings, respectively. 

**Invariants**: In every legal 10x10 draughts position, the following constraints are satisfied:
   - the `black` and `white` bitboards have an empty intersection (bitwise-and); 
   - every piece in the `kings` bitboard is also contained within the union (bitwise-or) of the `black` and `white` bitboards;
   - any `white` piece located on squares 1 through 5, and any `black` piece located on squares 46 through 50, is also present in the `kings` bitboard.       
    
---

### `egdb_interface::EGDB_COLOR`    
    enum EGDB_COLOR {
        EGDB_BLACK = 0,
        EGDB_WHITE = 1
    };

---

### `egdb_interface::WLD_VALUE`   
    enum WLD_VALUE {
        EGDB_SUBDB_UNAVAILABLE = -2,
        EGDB_NOT_IN_CACHE = -1,     
        EGDB_UNKNOWN = 0,           
        EGDB_WIN = 1,
        EGDB_LOSS = 2,
        EGDB_DRAW = 3,
        EGDB_DRAW_OR_LOSS = 4,
        EGDB_WIN_OR_DRAW = 5,
    };

**Notes**: enumerates integer values returned by `egdb_lookup()` on the WLD-database. The following values will be returned, depending on the position being queried:

  - If the corresponding database was not found or initialized during `edgb_open()` (e.g. because of limitations set by the `maxpieces` or `maxkings_1side_8pcs` options),  `EGDB_SUBDB_UNAVAILABLE` will be returned.
  - If the conditional lookup argument `cl` was non-zero, `EGDB_NOT_IN_CACHE` might be returned. 
  - If the side-to-move of a 7 or 8 piece position is excluded from the database, `EGDB_UNKNOWN` will be returned. 
  - For all regular position, `EGDB_WIN`, `EGDB_LOSS`, or `EGDB_DRAW` will be returned. 
  - For databases with incompete data, such as 5 men vs. 4 men, `EGDB_UNKNOWN`, `EGDB_DRAW_OR_LOSS`, or `EGDB_WIN_OR_DRAW` might also be returned. 


Note that depending on the position and the database type, you sometimes cannot call lookup for non-side capture positions, and you can never call lookup for capture positions, because the value returned will not be correct except by random luck. See the section "Excluded positions" for more details.

---

### `egdb_interface::MTC_VALUE`    
    enum MTC_VALUE {
        MTC_LESS_THAN_THRESHOLD = 1,
        MTC_THRESHOLD = 10,
    };
    
**Notes**: enumerates integer values returned by `egdb_lookup()` on the MTC database. The following values will be returned, depending on the position being queried:

  - For positions that are closer than `MTC_THRESHOLD` to a conversion move, `MTC_LESS_THAN_THRESHOLD` will be returned.
  - Otherwise, the number of plies to a conversion move will be returned.
    - The MTC databases do not store positions that are closer than `MTC_THRESHOLD` plies to a conversion move, because this allows the databases to be much smaller than if all positions were stored. It will only return even MTC values, because the database represents the MTC value internally by half the number of plies. The true value is either the returned `value` or `value - 1`. 
    - An application program can infer the true even or odd value of a position by looking up the MTC values of the position's successors. If the best successor MTC value is the same as the position's, then the position's true value is 1 less than the returned value.
 
---
      
## Core functions

### `egdb_interface::egdb_open`
    EGDB_DRIVER *egdb_open(
        char const *options, 
        int cache_mb, 
        char const *directory, 
        void (*msg_fn)(char const *msg)
    );

**Parameters**: 
  - `options`:  a character string of optional open settings. The options are of the form `name = value`, with multiple options separated by a semicolon (`;`) and either a `NULL` pointer or an empty string (`""`) can be given for no options. The following options are currently defined: 
    - `maxpieces = N`: sets the maximum number of pieces for which the driver will lookup values. By default, all the database files found during `egdb_open()` will be used. This can also be queried using `egdb_identify()`. 
    - `maxkings_1side_8pcs = N`: sets the maximum number of kings each side can have in 8-piece positions for which the driver will lookup values. By default, all 8-piece databases (if not otherwise restricted by `max_pieces`) will be used. 
  - `cache_mb`: the number of MiB (`2^20` bytes) of dynamically allocated memory that the driver will use for caching previously looked up positions. 
  - `directory`: the full path to the location of the database files.  
  - `msg_fn`: a function pointer that will receive status and error messages from the driver. 

**Effects**: opens a database, allocates memory for caching db values, and reads indexing files. The driver normally emits status messages when it starts up, with information about files opened and memory used. If any errors occur, either during the `egdb_open()` call or in subsequent calls through the driver `handle`, the details will be reported through the message function. Saving driver messages to a log file can be useful when diagnosing an unexpected problem. A message function that writes messages to stdout can be defined like this:

    void msg_fn(char const *msg)
    {
        printf("%s", msg);
    }

**Returns**: An `EGDB_DRIVER*` for subsequent communication with the driver. A `NULL` pointer return value means that an error occurred. 

**Complexity**: Linear in the size of the databases to be read in from disk.

**Notes**: Opening a db takes some time, but allows the db to be probed quickly during an engine search. Some of it is used for indexing data, and the rest is used to dynamically cache database data to minimize disk access during lookups. The more memory you give the driver, the faster it works on average during an engine search.

  - If you are opening the WLD database, give it the following values for `cache_mb`: 
    - for systems with at least 8 GiB of memory: the total amount memory minus 3 GiB 
    - for systems with less than 8 GiB of memory: around 1.5 GiB has been tested to perform well, but of course more memory is better.
  - If you are opening the MTC database, give it a `cache_mb` value of 0. It will then automatically be initialized to its required amount of memory, approximately 25 MiB. 
    - Unlike the WLD-database, it is not necessary to probe the MTC-database during a recursive search routine. It is sufficient to probe at the root of the recursive search, and collect the MTC values of the immediate successors of the target position. If at least one move can be obtained from the MTC-database then a further search is not necessary. 
    - The successor with the best MTC value is the lowest value for a won position, or the highest value for a lost position. 
    - To obtain a move from the MTC db it is necessary to skip probing of successors that were conversion moves from the target position. A conversion move is a capture or a man move.

---

### `egdb_interface::egdb_close`
    int egdb_close(
        EGDB_DRIVER *handle
    );

**Parameters**:
  - `handle`: an `EGDB_DRIVER*` returned by `egdb_open()`.

**Effects**: Closes the driver pointed to by `handle` and releases all associated resources. 

**Returns**: Zero on success, non-zero if there are any errors.

**Notes**: If an application needs to change anything about an endgame database driver after it has been opened, such as the maximum number of pieces or the megabytes of memory to use, it must be closed and then opened again with the new parameters. [ *Example:*

    // define dir and msg_fun
    EGDB_DRIVER* handle = egdb_open("", 2048, dir, msg_fn); // use 2 GiB of memory, all databases found  
    // various lookups
    egdb_close(handle);
    handle = egdb_open("maxpieces = 6", 1024, dir, msg_fn); // use 1 GiB of memory, at most 6-piece databases

*- end example* ]

---

### `egdb_interface::egdb_lookup`
    int egdb_lookup(
        EGDB_DRIVER *handle, 
        EGDB_POSITION const *position, 
        int color, 
        int cl
    );

**Parameters**:
  - `handle`: an `EGDB_DRIVER*` returned from `egdb_open()`.
  - `position`: a legal 10x10 international draughts position.
  - `color`: the side-to-move, either `EGDB_BLACK` or `EGDB_WHITE`.
  - `cl`: an integer indicating whether the driver should perform a conditional lookup.  
    - zero: always perform a lookup. If the position is not in cache, the driver will load a 4K block of data from disk into cache to get the position's value. 
    - non-zero: only perform a lookup if the position is already cached in memory, otherwise return `EGDB_NOT_IN_CACHE`. 

**Returns**: a database value.

  - when probing a WLD-database: an integer value as documented in `WLD_VALUE`.
  - when probing a MTC-database: an integer value as documented in `MTC_VALUE`. 


**Notes**: An example of a place to use conditional lookup is during quiescence search, where you don't want the search to be slowed by disk access. If the database is not on a fast SSD you might want to use conditional lookup in some other areas of a search to limit the slowing.

---

## Auxiliary functionality

### `egdb_interface::EGDB_TYPE`
    enum EGDB_TYPE {
        EGDB_WLD_RUNLEN = 0,
        EGDB_MTC_RUNLEN,
        EGDB_WLD_HUFFMAN,
        EGDB_WLD_TUN_V1,
        EGDB_WLD_TUN_V2,
    };

**Notes**: there is a single version of the MTC-database, and several versions of WLD databases, each using a different compression technique:
  - `EGDB_WLD_RUNLEN`: [runlength encoding](https://en.wikipedia.org/wiki/Run-length_encoding). This original format was only distributed to a limited number of Kingsrow testers, and is now deprecated.
  - `EGDB_MTC_RUNLEN`: runlength encoding. There are no other formats for the MTC-datbase.
  - `EGDB_WLD_HUFFMAN`: [Huffman coding](https://en.wikipedia.org/wiki/Huffman_coding). This experimental format used substantially less disk space at the expense of substantially more time during the engine search. For this reason, this version was never released.  
  - `EGDB_WLD_TUN_V1`: a mixture of [Tunstall coding](https://en.wikipedia.org/wiki/Tunstall_coding) and runlength encoding. The databases are 407gb of data, which includes a small subset of 9-piece positions: 5 men vs. 4 men. However, the 9-piece functionality is disabled in this driver because on average it turns out to be less effective during an engine search than not using it. From 2010 to 2014, these databases were bundled with the commercially available Kingsrow program, and distributed on a portable external hard drive. See also [this thread](http://laatste.info/bb3/viewtopic.php?t=2905) on the FMJD forum.
  - `EGDB_WLD_TUN_V2`: an optimized version of Tunstall coding, and also excluding more positions. The databases are 56gb of data. From 2014 onwards, these databases have been available for download from a file server. 

---

### `egdb_interface::egdb_identify`
    int egdb_identify(
        char const *directory, 
        EGDB_TYPE *egdb_type, 
        int *max_pieces
    );

**Parameters**:

  - `directory`: the full path to the location of the database files.
  - `egdb_type`: the endgame type.   
  - `max_pieces`: the database with the maximum number of pieces. 

**Effects**: Checks if an endgame database exists in `directory`. If so, the identified endgame database type is written into `egdb_type` and the maximum number of pieces for any of the databases identified is written into `max_pieces`. Otherwise, these out-parameters are unchanged on return.

**Returns**: Zero if a database is found, non-zero otherwise.

---

### `egdb_interface::EGDB_VERIFY_MSGS`
    struct EGDB_VERIFY_MSGS {
        char crc_failed[80];
        char ok[80];
        char errors[80];
        char no_errors[80];
    };

---

### `egdb_interface::egdb_verify`
    int egdb_verify(
        EGDB_DRIVER const *handle, 
        void (*msg_fn)(char const *msg), 
        int *abort, 
        EGDB_VERIFY_MSGS *msgs
    );

**Parameters**: 
  - `handle`: an `EGDB_DRIVER*` returned by `egdb_open()`.
  - `msg_fn`: a function pointer that will receive status and error messages from the driver. 
  - `abort`: if non-zero, cancels the verification process (because it can take a while). To abort verification, set the value of `*abort` to non-zero. 
  - `msgs`: an `EGDB_VERIFY_MSGS` struct of language localization messages used by verify.

**Effects**: checks that every index and data file associated to `handle` as a correct CRC value. If not, and an error message is written through the `msg_fn`. 

**Returns**: zero if all CRCs matched their known values, non-zero if there are any CRCs that did not match. 

---

### `egdb_interface::egdb_get_pieces`
    int egdb_get_pieces(
        EGDB_DRIVER const *handle, 
        int *max_pieces, 
        int *max_pieces_1side, 
        int *max_9pc_kings, 
        int *max_8pc_kings_1side
    );

`"get_pieces"` is a way to query some attributes of an open database.

---

## Deprecated functionality

**Notes**: The functions `egdb_get_stats()` and `egdb_reset_stats()` for accessing statistics about the database use are primarily for use by the driver developer and are deprecated in this public release of the driver. They may be removed in future releases.

---

### `egdb_interface::EGDB_STATS`    
    struct EGDB_STATS {
        unsigned int lru_cache_hits;
        unsigned int lru_cache_loads;
        unsigned int autoload_hits;
        unsigned int db_requests;            
        unsigned int db_returns;              
        unsigned int db_not_present_requests;
        float avg_ht_list_length;
    };

---

### `egdb_interface::egdb_get_stats`
    EGDB_STATS *egdb_get_stats(
        EGDB_DRIVER const *handle
    );
    
---

### `egdb_interface::egdb_reset_stats`
    void egdb_reset_stats(
        EGDB_DRIVER *handle
    );
    
---
