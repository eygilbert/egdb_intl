#pragma once
#include "engine/board.h"

namespace egdb_interface {

    typedef enum {
        DIR_LFRB, DIR_RFLB
    } DIR;

    typedef enum {
        DIR_LF, DIR_RF, DIR_LB, DIR_RB
    } DIR4;

    typedef struct {
        int lflen;
        int rflen;
        int lblen;
        int rblen;
        BITBOARD lf[8];
        BITBOARD rf[8];
        BITBOARD lb[8];
        BITBOARD rb[8];
    } DIAGS;


#define BLACK_KING_RANK_MASK ROW9
#define WHITE_KING_RANK_MASK ROW0


    /*
     *     Bit positions           Square numbers
     *
     *          white                   white
     *
     *    53  52  51  50  49      50  49  48  47  46
     *  48  47  46  45  44      45  44  43  42  41
     *    42  41  40  39  38      40  39  38  37  36
     *  37  36  35  34  33      35  34  33  32  31
     *    31  30  29  28  27      30  29  28  27  26
     *  26  25  24  23  22      25  24  23  22  21
     *    20  19  18  17  16      20  19  18  17  16
     *  15  14  13  12  11      15  14  13  12  11
     *    09  08  07  06  05      10  09  08  07  06
     *  04  03  02  01  00      05  04  03  02  01
     *
     *         black                    black
     */

#define LEFT_FWD_SHIFT 6
#define RIGHT_BACK_SHIFT 6
#define RIGHT_FWD_SHIFT 5
#define LEFT_BACK_SHIFT 5

}   // namespace egdb_interface
