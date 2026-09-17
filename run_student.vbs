' =============================================================================
' ClassroomMonitor - Silent Student Agent Launcher (Zero Console Window)
' =============================================================================
Option Explicit

Dim WshShell, FSO, ScriptDir, ExePath, ServerIp, ArgStr, i

Set WshShell = CreateObject("WScript.Shell")
Set FSO      = CreateObject("Scripting.FileSystemObject")

ScriptDir = FSO.GetParentFolderName(WScript.ScriptFullName)
WshShell.CurrentDirectory = ScriptDir

' Check if arguments were passed
ArgStr = ""
If WScript.Arguments.Count > 0 Then
    For i = 0 To WScript.Arguments.Count - 1
        ArgStr = ArgStr & " """ & WScript.Arguments(i) & """"
    Next
ElseIf FSO.FileExists(ScriptDir & "\server_ip.txt") Then
    Dim ipFile, line
    On Error Resume Next
    Set ipFile = FSO.OpenTextFile(ScriptDir & "\server_ip.txt", 1)
    If Not ipFile Is Nothing Then
        If Not ipFile.AtEndOfStream Then
            line = Trim(ipFile.ReadLine)
            If Len(line) > 0 Then
                ArgStr = " " & line
            End If
        End If
        ipFile.Close
    End If
    On Error GoTo 0
End If

' Find StudentAgent.exe
ExePath = ""
If FSO.FileExists(ScriptDir & "\build\Debug\StudentAgent.exe") Then
    ExePath = ScriptDir & "\build\Debug\StudentAgent.exe"
ElseIf FSO.FileExists(ScriptDir & "\build\Release\StudentAgent.exe") Then
    ExePath = ScriptDir & "\build\Release\StudentAgent.exe"
ElseIf FSO.FileExists(ScriptDir & "\bin\StudentAgent.exe") Then
    ExePath = ScriptDir & "\bin\StudentAgent.exe"
ElseIf FSO.FileExists(ScriptDir & "\StudentAgent.exe") Then
    ExePath = ScriptDir & "\StudentAgent.exe"
End If

If ExePath <> "" Then
    ' Run StudentAgent completely hidden (0 = SW_HIDE, False = async)
    WshShell.Run """" & ExePath & """" & ArgStr, 0, False
Else
    ' Not compiled yet - run build first (visible) then run agent silently
    WshShell.Run "cmd /c build.bat", 1, True
    If FSO.FileExists(ScriptDir & "\build\Debug\StudentAgent.exe") Then
        WshShell.Run """" & ScriptDir & "\build\Debug\StudentAgent.exe""" & ArgStr, 0, False
    ElseIf FSO.FileExists(ScriptDir & "\build\Release\StudentAgent.exe") Then
        WshShell.Run """" & ScriptDir & "\build\Release\StudentAgent.exe""" & ArgStr, 0, False
    Else
        MsgBox "Не удалось найти скомпилированный StudentAgent.exe. Запустите build.bat для сборки проекта.", 16, "ClassroomMonitor - Ошибка"
    End If
End If
