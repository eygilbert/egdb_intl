Attribute VB_Name = "bitboards"
Option Explicit
Public bb_to_bitnum_map(2 ^ 10) As Byte

'Initialize the public array that maps a bitboard with a single bit set
'between bit 0 and bit 9 to the bit number.
Sub init_bb_to_bitnum_map()
Dim index As Long, i As Long
For i = 0 To 9
 index = 2 ^ i
 bb_to_bitnum_map(index) = i
Next i
End Sub

'Given a bitboard with an arbitrary number of bits set, return a bitboard that
'has only one bit set, the lowest bit that is set in the input bitboard.
Public Function get_lsb(ByVal val As LongLong) As LongLong
get_lsb = val And -val
End Function

'Clear the lowest set bit in a bitboard
Public Function clear_lsb(ByVal val As LongLong) As LongLong
clear_lsb = val And (val - 1)
End Function

'Convert a bitboard with a single bit set to its square number.
Function bb_to_square(ByVal val As LongLong) As Byte
If val >= 2 ^ 44 Then
  val = val / (2 ^ 44)
  bb_to_square = 41 + bb_to_bitnum_map(CInt(val))
ElseIf val >= 2 ^ 33 Then
  val = val / (2 ^ 33)
  bb_to_square = 31 + bb_to_bitnum_map(CInt(val))
ElseIf val >= 2 ^ 22 Then
  val = val / (2 ^ 22)
  bb_to_square = 21 + bb_to_bitnum_map(CInt(val))
ElseIf val >= 2 ^ 11 Then
  val = val / (2 ^ 11)
  bb_to_square = 11 + bb_to_bitnum_map(CInt(val))
Else
  bb_to_square = 1 + bb_to_bitnum_map(CInt(val))
End If
End Function

'Simple bitboard test. Create a position of type Board, then use the
'bitboard functions to print out the squares represented
'by the black and white pieces.
Sub test_bitboards()
Dim pos As Board, color As Long, stat As Long
Dim pieces As LongLong, piece As LongLong
Call init_bb_to_bitnum_map
stat = fentoposition("B:W5,15,27:B38,49", pos, color)
pieces = pos.black Or pos.white
Do Until pieces = 0
  piece = get_lsb(pieces)
  pieces = clear_lsb(pieces)
  Debug.Print "square "; bb_to_square(piece)
Loop
End Sub
