@echo off
rem Opens the LBPPC launcher (launcher\lbppc_launcher.py) without a console window. Development builds use
rem MSYS2's CLANG64 Python, the one the build uses; any Python 3 with Tk works.
cd /d "%~dp0"
if exist "C:\msys64\clang64\bin\pythonw.exe" (
    start "" "C:\msys64\clang64\bin\pythonw.exe" "%~dp0launcher\lbppc_launcher.py"
) else (
    start "" pythonw "%~dp0launcher\lbppc_launcher.py"
)
