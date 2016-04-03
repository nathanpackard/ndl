SET LIBPATH=..\..\lib
SET BINPATH=bin64

REM DEFINE LIBRARY PATHS
SET WXLIBS=%LIBPATH%\wxwidgets\lib64\wxmsw28_core.lib %LIBPATH%\wxwidgets\lib64\wxmsw28_xrc.lib %LIBPATH%\wxwidgets\lib64\wxbase28.lib  %LIBPATH%\wxwidgets\lib64\wxmsw28_adv.lib  %LIBPATH%\wxwidgets\lib64\wxmsw28_richtext.lib  %LIBPATH%\wxwidgets\lib64\wxbase28_net.lib User32.lib Gdi32.lib shell32.lib Ole32.lib Oleaut32.lib comctl32.lib Comdlg32.lib Advapi32.lib
REM ~ SET OPENMPLIB=%LIBPATH%/openmp/lib64/VCOMP.lib

REM DEFINE FLAGS, LIBS, AND INCLUDES

REM ~ SET MYFLAGS=/DWIN32 /openmp /DNDL_USE_OMP /DNDL_USE_BCTLIB /DNDL_USE_WXWIDGETS
SET MYFLAGS=/DWIN32 /DNDL_USE_BCTLIB /DNDL_USE_WXWIDGETS

REM ~ SET MYLIBS=%WXLIBS% %OPENMPLIB%
SET MYLIBS=%WXLIBS%
SET MYINC=/I%LIBPATH%\bctlib /I%LIBPATH%\ndl /I%LIBPATH%\wxwidgets /I%LIBPATH%\wxwidgets\msw

REM DEFINE COMPILE COMMAND AND FLAGS
SET MYCPP=cl /Ox /EHsc /D_WIN32_WINNT=0x0400 /MD 
SET MYLINKFLAGS=/NODEFAULTLIB:LIBCMT

cls
%MYCPP% %MYFLAGS% %MYINC% ndlrecon.cpp /link %MYLIBS% %MYLINKFLAGS%
move *.exe %BINPATH%
move *.manifest %BINPATH%
del *.obj