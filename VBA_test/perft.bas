Attribute VB_Name = "perft"
Option Explicit
Function perft(ByRef pos As Board, ByVal color As Long, ByVal depth As Long) As LongLong
    Dim nmoves As Long, i As Long, nextcolor As Long
    Dim nodes As LongLong
    Dim movelist(128) As Board
    
    nmoves = get_movelist(pos, color, movelist(0))
    If depth = 1 Then
        perft = nmoves
        Exit Function
    End If
    nodes = 0
    nextcolor = other_color(color)
    For i = 0 To nmoves - 1
        nodes = nodes + perft(movelist(i), nextcolor, depth - 1)
    Next i
    perft = nodes
    If depth > 7 Then DoEvents  'Allow VBA UI to be responsive
End Function

Function root_perft(ByRef fen As String, ByVal depth As Long, ByVal expected_nodes As LongLong) As LongLong
    Dim status As Long
    Dim pos As Board
    Dim color As Long
    Dim i As Long
    Dim nodes As LongLong
    Dim t0 As Date, t1 As Date
    Dim tdiff As Double, speed As Long
    
    status = fentoposition(fen, pos, color)
    Debug.Print fen
    For i = 1 To depth
        t0 = Now
        nodes = perft(pos, color, i)
        t1 = Now
        tdiff = DateDiff("s", t0, t1)
        If tdiff = 0 Then
            speed = 0
        Else
            speed = nodes / (1000 * tdiff)
        End If
        Debug.Print ("Perft(" & i & ") " & nodes & " nodes, " & tdiff & " sec, " & speed & " knodes/sec")
    Next i
    root_perft = nodes
    If expected_nodes <> 0 And nodes <> expected_nodes Then
        Debug.Print "expected " & expected_nodes & ", got " & nodes
    End If
    Debug.Print
End Function


Sub Perft_test()
    Dim nodes As LongLong
    nodes = root_perft("W:W31-50:B1-20", 9, 41022423)
    nodes = root_perft("B:W6,9,10,11,20,21,22,23,30,K31,33,37,41,42,43,44,46:BK17,K24", 8, 86724219)
    nodes = root_perft("W:W25,27,28,30,32,33,34,35,37,38:B12,13,14,16,18,19,21,23,24,26", 14, 58360286)
    nodes = root_perft("W:WK31-50:BK1-20", 10, 58381162)
    nodes = root_perft("W:W6-10:B41-45", 8, 138362698)
End Sub
