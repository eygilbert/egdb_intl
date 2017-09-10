VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "CSlice"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
Option Explicit

' CLASS MODULE CODE

' Member variable
Private m_npieces As Long
Private m_nb As Long
Private m_nw As Long
Private m_nbm As Long, m_nbk As Long, m_nwm As Long, m_nwk As Long
Private m_maxpieces As Long, m_maxpiece As Long

Property Get nbm() As Long
    nbm = m_nbm
End Property

Property Get nbk() As Long
    nbk = m_nbk
End Property

Property Get nwm() As Long
    nwm = m_nwm
End Property

Property Get nwk() As Long
    nwk = m_nwk
End Property

Property Get npieces() As Long
    npieces = m_npieces
End Property


Private Sub Class_Initialize()
    m_maxpieces = 8
    m_maxpiece = 5
    init_npieces (2)
End Sub

Private Function min(ByVal v1 As Long, ByVal v2 As Long) As Long
    If v1 <= v2 Then
        min = v1
    Else
        min = v2
    End If
End Function

Public Sub init_npieces(ByVal npieces As Long)
    m_npieces = npieces
    m_nb = min(m_npieces - 1, m_maxpiece)
    m_nw = m_npieces - m_nb
    m_nbm = 0
    m_nbk = m_nb
    m_nwm = 0
    m_nwk = m_nw
End Sub

'Increment piece counts to the next slice.
'Return false if at the end and cannot increment.
Public Function increment() As Boolean
    increment = True
    If m_nbk > 0 Then
        m_nbk = m_nbk - 1
        m_nbm = m_nbm + 1
        Exit Function
    End If
    If m_nwk > 0 Then
        m_nwk = m_nwk - 1
        m_nwm = m_nwm + 1
        m_nbm = 0
        m_nbk = m_nb
        Exit Function
    End If
    
    'Increase nw until limit
    If m_nw < m_maxpiece And m_nb > 1 Then
        m_nw = m_nw + 1
        m_nb = m_nb - 1
        m_nbm = 0
        m_nbk = m_nb
        m_nwm = 0
        m_nwk = m_nw
        Exit Function
    End If
    
    'Increase npieces until limit
    If m_npieces < m_maxpieces Then
        init_npieces (m_npieces + 1)
        Exit Function
    End If
    increment = False
End Function
