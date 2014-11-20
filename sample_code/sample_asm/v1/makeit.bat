rem @echo off

set FN=v1

if exist %FN%.obj del %FN%.obj
if exist %FN%.exe del %FN%.exe

\masm32\bin\ml /c /coff /nologo %FN%.asm

\masm32\bin\Link /SUBSYSTEM:WINDOWS /BASE:0x401000 /MERGE:.rdata=.text /SECTION:.text,RWX %FN%.obj

dir %FN%.*

pause
