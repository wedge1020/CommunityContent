@echo off
REM Build script for the Gamebuino Classic -> Vircon32 port.
REM Assumes compile.exe, assemble.exe, png2vircon.exe, wav2vircon.exe and
REM packrom.exe are on PATH (same layout as this author's other Vircon32
REM ports).

if not exist obj mkdir obj
if not exist bin mkdir bin

echo.
echo Compile the C code
echo --------------------------
compile src\main.c -o obj\main.asm || goto :failed

echo.
echo Optimize the ASM code - v32opt, optional
echo --------------------------
set ASM_TO_ASSEMBLE=obj\main.asm
if "%SKIP_V32OPT%"=="1" (
    echo SKIP_V32OPT=1 set - skipping, using unoptimized assembly
) else (
    where v32opt >nul 2>nul
    if %errorlevel% equ 0 (
        v32opt obj\main.asm -o obj\main_opt.asm -v -O3 && set ASM_TO_ASSEMBLE=obj\main_opt.asm
    ) else (
        echo v32opt not found on PATH - skipping, using unoptimized assembly
    )
)

echo.
echo Assemble the ASM code
echo --------------------------
assemble %ASM_TO_ASSEMBLE% -o obj\main.vbin || goto :failed

echo.
echo Convert the PNG textures
echo --------------------------
png2vircon assets\columns.png -o obj\columns.vtex || goto :failed
png2vircon assets\thumbnails.png -o obj\thumbnails.vtex || goto :failed
png2vircon assets\pixelgrid.png -o obj\pixelgrid.vtex || goto :failed
png2vircon assets\thumbnails2.png -o obj\thumbnails2.vtex || goto :failed
png2vircon assets\columns_gray.png -o obj\columns_gray.vtex || goto :failed
png2vircon assets\thumbnails3.png -o obj\thumbnails3.vtex || goto :failed
png2vircon assets\thumbnails4.png -o obj\thumbnails4.vtex || goto :failed

echo.
echo Convert the PlayNote wavetable
echo --------------------------
wav2vircon libs\PlayNote\sounds\wt_square.wav -o obj\wt_square.vsnd || goto :failed

echo.
echo Pack the ROM
echo --------------------------
packrom rom.xml -o "bin\gamebuino_classic.v32" || goto :failed

goto :succeeded

:failed
echo.
echo BUILD FAILED
exit /b %errorlevel%

:succeeded
echo.
echo BUILD SUCCESSFUL
exit /b

@echo on
