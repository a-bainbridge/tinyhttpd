@echo off
tcc -g main.c win-net.c http.c settings.c -o httpd.exe
httpd.exe -v --port=80