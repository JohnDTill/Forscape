chcp 65001
@echo off
Setlocal enabledelayedexpansion

Set "Pattern=txt"
Set "Replace=π"

For %%a in (*.*) Do (
    Set "File=%%~a"
    Ren "%%a" "!File:%Pattern%=%Replace%!"
)

Pause&Exit