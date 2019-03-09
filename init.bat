@echo off

IF NOT DEFINED isInitted (
  set VSCMD_START_DIR=%CD%
  call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
)

set isInitted=1