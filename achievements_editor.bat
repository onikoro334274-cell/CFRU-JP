@echo off
cd /d "%~dp0"
python -I scripts\achievements_editor.py
if errorlevel 1 pause
