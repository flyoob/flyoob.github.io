@echo off
for /r %%a in (*.txt) do  ren "%%~sa" "*."
