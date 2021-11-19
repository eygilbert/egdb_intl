Attribute VB_Name = "egdb_dll_gbrules"
Option Explicit

'Color values
Public Const EGDB_BLACK = 0
Public Const EGDB_WHITE = 1

'Values returned by egdb_lookup_fen_with_search()
Public Const EGDB_SUBDB_UNAVAILABLE As Long = -2       'this slice is not being used
Public Const EGDB_NOT_IN_CACHE As Long = -1            'conditional lookup and position not in cache.
Public Const EGDB_UNKNOWN As Long = 0                  'value not in the database.
Public Const EGDB_WIN As Long = 1
Public Const EGDB_LOSS As Long = 2
Public Const EGDB_DRAW As Long = 3
 
'egdb_type values returned by egdb_identify()
Public Const EGDB_WLD_RUNLEN = 0
Public Const EGDB_MTC_RUNLEN = 1
Public Const EGDB_WLD_HUFFMAN = 2
Public Const EGDB_WLD_TUN_V1 = 3
Public Const EGDB_WLD_TUN_V2 = 4

'Values returned by sharp_status()
Public Const EGDB_SHARP_STAT_TRUE = 1
Public Const EGDB_SHARP_STAT_FALSE = 0
Public Const EGDB_SHARP_STAT_UNKNOWN = -1
 
'DLL function error return values
Public Const EGDB_DRIVER_TABLE_FULL As Long = -100
Public Const EGDB_FOPEN_FAIL As Long = -101
Public Const EGDB_OPEN_FAIL As Long = -102
Public Const EGDB_IDENTIFY_FAIL As Long = -103
Public Const EGDB_BAD_HANDLE As Long = -104
Public Const EGDB_NULL_INTERNAL_HANDLE As Long = -105
Public Const EGDB_FEN_ERROR As Long = -106
 
#If Win64 Then
Type Board
    black As LongLong
    white As LongLong
    king As LongLong
End Type

'External DLL functions
Declare PtrSafe Function egdb_identify Lib "egdb_intl64_gbrules" (ByVal directory As String, ByRef egdb_type As Long, ByRef max_pieces As Long) As Long
Declare PtrSafe Function egdb_open Lib "egdb_intl64_gbrules" (ByVal options As String, ByVal cache_mb As Long, ByVal directory As String, ByVal filename As String) As Long
Declare PtrSafe Function egdb_close Lib "egdb_intl64_gbrules" (ByVal handle As Long) As Long
Declare PtrSafe Function egdb_lookup_fen_with_search Lib "egdb_intl64_gbrules" (ByVal handle As Long, ByVal fen As String) As Long
Declare PtrSafe Function egdb_lookup_with_search Lib "egdb_intl64_gbrules" (ByVal handle As Long, ByRef pos As Board, ByVal color As Long) As Long
Declare PtrSafe Function get_movelist Lib "egdb_intl64_gbrules" (ByRef pos As Board, ByVal color As Long, ByRef movelist As Board) As Long
Declare PtrSafe Function is_capture Lib "egdb_intl64_gbrules" (ByRef pos As Board, ByVal color As Long) As Boolean
Declare PtrSafe Function getdatabasesize_slice Lib "egdb_intl64_gbrules" (ByVal nbm As Long, ByVal nbk As Long, ByVal nwm As Long, ByVal nwk As Long) As LongLong
Declare PtrSafe Sub indextoposition Lib "egdb_intl64_gbrules" (ByVal index As LongLong, ByRef pos As Board, ByVal nbm As Long, ByVal nbk As Long, ByVal nwm As Long, ByVal nwk As Long)
Declare PtrSafe Function positiontoindex Lib "egdb_intl64_gbrules" (ByRef pos As Board, ByVal nbm As Long, ByVal nbk As Long, ByVal nwm As Long, ByVal nwk As Long) As LongLong
Declare PtrSafe Function sharp_status Lib "egdb_intl64_gbrules" (ByVal handle As Long, ByRef pos As Board, ByVal color As Long, ByRef sharp_move_pos As Board) As Long
Declare PtrSafe Function move_string Lib "egdb_intl64_gbrules" (ByRef oldpos As Board, ByRef newpos As Board, ByVal color As Long, ByRef move As Byte) As Long
Declare PtrSafe Function capture_path Lib "egdb_intl64_gbrules" (ByRef oldpos As Board, ByRef newpos As Board, ByVal color As Long, ByRef landed As Byte, ByRef captured As Byte) As Long
Declare PtrSafe Function positiontofen Lib "egdb_intl64_gbrules" (ByRef pos As Board, ByVal color As Long, ByRef fen As Byte) As Long
Declare PtrSafe Function fentoposition Lib "egdb_intl64_gbrules" (ByVal fen As String, ByRef pos As Board, ByRef color As Long) As Long

#Else
Declare Function egdb_identify Lib "egdb_intl32_gbrules" (ByVal directory As String, ByRef egdb_type As Long, ByRef max_pieces As Long) As Long
Declare Function egdb_open Lib "egdb_intl32_gbrules" (ByVal options As String, ByVal cache_mb As Long, ByVal directory As String, ByVal filename As String) As Long
Declare Function egdb_close Lib "egdb_intl32_gbrules" (ByVal handle As Long) As Long
Declare Function egdb_lookup_fen_with_search Lib "egdb_intl32_gbrules" (ByVal handle As Long, ByVal fen As String) As Long
#End If

'Given "from" and "to" boards, and the side-to-move color, return a string representation of the move.
'
Function movestr(ByRef oldpos As Board, ByRef newpos As Board, ByVal color As Long) As String
    Dim move(100) As Byte
    Dim movs As String
    Dim length As Long, i As Long
    
    length = move_string(oldpos, newpos, color, move(0))
    For i = 0 To length - 1
        movs = movs & Chr(move(i))
    Next i
    movestr = movs
End Function


Function postofen(ByRef pos As Board, ByVal color As Long) As String
    Dim fen(200) As Byte
    Dim fenstr As String
    Dim length As Long, i As Long
    
    length = positiontofen(pos, color, fen(0))
    For i = 0 To length - 1
        fenstr = fenstr & Chr(fen(i))
    Next i
    postofen = fenstr
End Function


Function other_color(ByVal color As Long) As Long
    If color = EGDB_BLACK Then
        other_color = EGDB_WHITE
    Else
        other_color = EGDB_BLACK
    End If
End Function
