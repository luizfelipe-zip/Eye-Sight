@echo off
rem Pede permissao de administrador e roda uninstall.ps1.
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process powershell -Verb RunAs -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File ' + [char]34 + '%~dp0uninstall.ps1' + [char]34)"
