Attribute VB_Name = "test_dll"
Option Explicit
Public Const max_captures = 20
Type Test_position
    oldfen As String
    newfen As String
    ncaptured As Byte
    landed(max_captures) As Byte
    captured(max_captures) As Byte
End Type

Sub quick_test()
    Dim egdb_type As Long
    Dim max_pieces As Long
    Dim result As Long, errors As Long
    Dim handle As Long
    Dim value As Long
    Const directory As String = "C:\db_intl\wld_v2"
        
    'Check that the driver finds the db files.
    result = egdb_identify(directory, egdb_type, max_pieces)
    
    'Open the driver, get a driver handle.
    Debug.Print "waiting for egdb_open()..."
    handle = egdb_open("maxpieces=6", 1000, directory, "")
    If handle < 0 Or handle > 3 Then
        MsgBox "egdb_open failed"
        Exit Sub
    Else
        Debug.Print "egdb_open complete"
    End If
    
    'Read a few WLD values.
    value = egdb_lookup_fen_with_search(handle, "B:WK28,31,32,50:B3,8")
    If value <> EGDB_LOSS Then
        MsgBox "expected " & EGDB_LOSS & ", got " & value
        errors = errors + 1
    End If
    value = egdb_lookup_fen_with_search(handle, "W:WK21,K25,K50:B4,42")
    If value <> EGDB_WIN Then
        MsgBox "expected " & EGDB_WIN & ", got " & value
        errors = errors + 1
    End If
    value = egdb_lookup_fen_with_search(handle, "W:W42,43,44:B7,18,20")
    If value <> EGDB_DRAW Then
        MsgBox "expected " & EGDB_DRAW & ", got " & value
        errors = errors + 1
    End If
    
    'Close the driver.
    result = egdb_close(handle)    'Should return result 0
    
    If errors = 0 Then
       MsgBox "No errors"
    End If
End Sub

'Check that a position's WLD value is consistent with the values of its successors.
Sub verify_position(ByVal handle As Long, pos As Board, ByVal color As Long)
    Dim pvalue As Long, value As Long, bestvalue As Long
    Dim i As Long
    Dim movelist(128) As Board, count As Long

    pvalue = egdb_lookup_with_search(handle, pos, color)
    count = get_movelist(pos, color, movelist(0))
    bestvalue = EGDB_LOSS
    For i = 0 To count - 1
        value = egdb_lookup_with_search(handle, movelist(i), other_color(color))
        Select Case value
        Case EGDB_LOSS
            bestvalue = EGDB_WIN
            Exit For
        Case EGDB_DRAW
            bestvalue = EGDB_DRAW
        Case EGDB_WIN
            'nothing to do here
        Case EGDB_UNKNOWN
            Exit Sub
        End Select
    Next i
    If pvalue <> bestvalue Then
        Debug.Print "egdb value verify error"
    End If
End Sub

Sub test_sharp_status(ByVal handle As Long, ByRef pos As Board, ByVal color As Long)
    Dim movelist(128) As Board, count As Long
    Dim destination As Board
    Dim value As Long, wincount As Long, i As Long, status As Long
    
    value = egdb_lookup_with_search(handle, pos, color)
    If value = EGDB_WIN Then
        count = get_movelist(pos, color, movelist(0))
        For i = 0 To count - 1
            If egdb_lookup_with_search(handle, movelist(i), other_color(color)) = EGDB_LOSS Then
                wincount = wincount + 1
            End If
        Next i
        status = sharp_status(handle, pos, color, destination)
        If status = EGDB_SHARP_STAT_TRUE Then
            If wincount <> 1 Then
                MsgBox ("Error testing sharp_status()")
            End If
        ElseIf status = EGDB_SHARP_STAT_FALSE Then
            If wincount = 1 Then
                MsgBox ("Error testing sharp_status()")
            End If
        ElseIf status = EGDB_SHARP_STAT_UNKNOWN Then
            MsgBox ("sharp_status() returned EGDB_SHARP_STAT_UNKNOWN")
        End If
    Else
        If sharp_status(handle, pos, color, destination) <> EGDB_SHARP_STAT_FALSE Then
            MsgBox ("Error testing sharp_status()")
        End If
    End If
End Sub

Sub test_fen_conversions(ByRef pos As Board, ByVal color As Long)
    Dim retpos As Board
    Dim retcolor As Long, status As Long
    Dim fenstr As String
    
    fenstr = postofen(pos, color)
    status = fentoposition(fenstr, retpos, retcolor)
    If retcolor <> color Then
        MsgBox ("fen test color error")
    End If
    If pos.black <> retpos.black Or pos.white <> retpos.white Or pos.king <> retpos.king Then
        MsgBox ("fen test position error")
    End If
End Sub

Sub test_one_slice(ByVal handle As Long, ByVal nbm As Long, ByVal nbk As Long, ByVal nwm As Long, ByVal nwk As Long)
    Dim pvalue As Long, value As Long
    Dim dbsize As LongLong, index As LongLong, retindex
    Dim pos As Board
    dbsize = getdatabasesize_slice(nbm, nbk, nwm, nwk)
    For index = 0 To dbsize - 1 Step dbsize / 100        'Test 100 positions from each slice
        Call indextoposition(index, pos, nbm, nbk, nwm, nwk)
        
        'Verify round trip index->pos->index.
        retindex = positiontoindex(pos, nbm, nbk, nwm, nwk)
        If index <> retindex Then
            Debug.Print "return index error, expected "; index; ", got "; retindex
        End If
        
        'Verify the position's egdb value
        Call verify_position(handle, pos, EGDB_BLACK)
        Call verify_position(handle, pos, EGDB_WHITE)
        
        'Test the sharp_status() dll function.
        Call test_sharp_status(handle, pos, EGDB_WHITE)
        
        Call test_fen_conversions(pos, EGDB_BLACK)
        Call test_fen_conversions(pos, EGDB_WHITE)
        DoEvents  'Allow VBA UI to be responsive
    Next index
End Sub

Sub test_sharp_capture_path()
Dim test_positions(2) As Test_position
Dim oldpos As Board, newpos As Board
Dim i As Long, j As Long, ncaptured As Long, color As Long, status As Long
Dim landed(max_captures) As Byte, captured(max_captures) As Byte

test_positions(0).oldfen = "W:WK49:B9,38"
test_positions(0).newfen = "B:WK4:B"
test_positions(0).ncaptured = 2
test_positions(0).landed(0) = 49
test_positions(0).landed(1) = 27
test_positions(0).landed(2) = 4
test_positions(0).captured(0) = 38
test_positions(0).captured(1) = 9
test_positions(1).oldfen = "W:WK49:B21,38"
test_positions(1).newfen = "B:WK16:B"
test_positions(1).ncaptured = 0
test_positions(2).oldfen = "W:WK49:B33,34,43,44"
test_positions(2).newfen = "B:WK49:B"
test_positions(2).ncaptured = 0
Debug.Print "Testing sharp_capture_path()"
For i = 0 To UBound(test_positions)
    status = fentoposition(test_positions(i).newfen, newpos, color)
    status = fentoposition(test_positions(i).oldfen, oldpos, color)
    ncaptured = sharp_capture_path(oldpos, newpos, color, landed(0), captured(0))
    If (ncaptured <> test_positions(i).ncaptured) Then
        Debug.Print "test_sharp_capture_path error"
        Exit For
    End If
    If ncaptured <> 0 Then
        For j = 0 To ncaptured
            If (test_positions(i).landed(j) <> landed(j)) Then
                Debug.Print "landed error, got "; landed(j); ", expected "; test_positions(i).landed(j)
                Exit For
            End If
            If j = ncaptured Then
                Exit For
            End If
            
            If (test_positions(i).captured(j) <> captured(j)) Then
                Debug.Print "captured error, got "; captured(j); ", expected "; test_positions(i).captured(j)
                Exit For
            End If
        Next j
    End If
Next i
End Sub

'Do a functional test of most of the dll functions.
Sub test_everything()
    Const directory As String = "C:\db_intl\wld_v2"
    Const maxpieces As Long = 8
    Dim handle As Long, result As Long
    Dim slice As New CSlice
    
    'Open the driver, get a driver handle.
    Debug.Print "waiting for egdb_open..."
    handle = egdb_open("maxpieces=" & maxpieces, 1000, directory, "")
    If handle < 0 Or handle > 3 Then
        MsgBox "egdb_open failed"
        Exit Sub
    End If
    
    Call test_sharp_capture_path
    
    'Iterate through all the slices, test each one.
    Do
        If slice.npieces > maxpieces Then Exit Do
        Debug.Print "db" & slice.nbm & slice.nbk & slice.nwm & slice.nwk
        Call test_one_slice(handle, slice.nbm, slice.nbk, slice.nwm, slice.nwk)
    Loop While slice.increment()
    result = egdb_close(handle)
End Sub

