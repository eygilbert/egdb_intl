Option Compare Database

'Values returned by egdb_lookup_fen_with_search()
Const EGDB_SUBDB_UNAVAILABLE As Long = -2       'this slice is not being used
Const EGDB_NOT_IN_CACHE As Long = -1            'conditional lookup and position not in cache.
Const EGDB_UNKNOWN As Long = 0                  'value not in the database.
Const EGDB_WIN As Long = 1
Const EGDB_LOSS As Long = 2
Const EGDB_DRAW As Long = 3
 
'egdb_type values returned by egdb_identify()
Const EGDB_WLD_RUNLEN = 0
Const EGDB_MTC_RUNLEN = 1
Const EGDB_WLD_HUFFMAN = 2
Const EGDB_WLD_TUN_V1 = 3
Const EGDB_WLD_TUN_V2 = 4
 
'DLL function error return values
Const EGDB_DRIVER_TABLE_FULL As Long = -100
Const EGDB_FOPEN_FAIL As Long = -101
Const EGDB_OPEN_FAIL As Long = -102
Const EGDB_IDENTIFY_FAIL As Long = -103
Const EGDB_BAD_HANDLE As Long = -104
Const EGDB_NULL_INTERNAL_HANDLE As Long = -105
Const EGDB_FEN_ERROR As Long = -106
 
'External DLL functions
#If Win64 Then
Declare PtrSafe Function egdb_identify Lib "egdb_intl64" (ByVal directory As String, ByRef egdb_type As Long, ByRef max_pieces As Long) As Long
Declare PtrSafe Function egdb_open Lib "egdb_intl64" (ByVal options As String, ByVal cache_mb As Long, ByVal directory As String, ByVal filename As String) As Long
Declare PtrSafe Function egdb_close Lib "egdb_intl64" (ByVal handle As Long) As Long
Declare PtrSafe Function egdb_lookup_fen_with_search Lib "egdb_intl64" (ByVal handle As Long, ByVal fen As String) As Long
#Else
Declare Function egdb_identify Lib "egdb_intl32" (ByVal directory As String, ByRef egdb_type As Long, ByRef max_pieces As Long) As Long
Declare Function egdb_open Lib "egdb_intl32" (ByVal options As String, ByVal cache_mb As Long, ByVal directory As String, ByVal filename As String) As Long
Declare Function egdb_close Lib "egdb_intl32" (ByVal handle As Long) As Long
Declare Function egdb_lookup_fen_with_search Lib "egdb_intl32" (ByVal handle As Long, ByVal fen As String) As Long
#End If

Sub TestDll()
    Dim egdb_type As Long
    Dim max_pieces As Long
    Dim result As Long
    Dim handle As Long
    Dim value As Long
    Const directory As String = "C:\db_intl\wld_v2"
  
    'Check that the driver finds the db files.
    result = egdb_identify(directory, egdb_type, max_pieces)
    
    'Open the driver, get a driver handle.
    handle = egdb_open("maxpieces=6", 1000, directory, "")
    
    'Read a few WLD values.
    value = egdb_lookup_fen_with_search(handle, "B:WK28,31,32,50:B3,8")  'should return value 2 (EGDB_LOSS).
    value = egdb_lookup_fen_with_search(handle, "W:WK21,K25,K50:B4,42")  'should return value 1 (EGDB_WIN).
    value = egdb_lookup_fen_with_search(handle, "W:W42,43,44:B7,18,20")  'should return value 3 (EGDB_DRAW).
    
    'Close the driver.
    result = egdb_close(handle)    'Should return result 0
End Sub

