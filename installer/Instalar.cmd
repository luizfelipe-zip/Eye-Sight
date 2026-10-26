@echo off
rem Pede permissao de administrador e roda install.ps1.
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process powershell -Verb RunAs -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File ' + [char]34 + '%~dp0install.ps1' + [char]34)"
