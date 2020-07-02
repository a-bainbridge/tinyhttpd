@echo off
tcc -g http.c -o httpd.exe
httpd.exe -v --port=80