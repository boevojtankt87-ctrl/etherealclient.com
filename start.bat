@echo off
cd /d %~dp0
echo Ethereal server starting...
python server\server.py
pause
