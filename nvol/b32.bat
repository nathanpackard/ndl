SET LIBPATH=..\..\lib
SET BINPATH=bin32
SET MYCPP=cl /Ox /EHsc /D_WIN32_WINNT=0x0400 /MD /openmp /DNDL_USE_OMP
REM ~ SET MYCPP=cl /Ox /EHsc /D_WIN32_WINNT=0x0400 /MD /DNDL_USE_PPL
SET MYLINKFLAGS=/NODEFAULTLIB:LIBCMT,libc
SET MYINC=/I%LIBPATH%\bctlib /I%LIBPATH%\analyzelib /I%LIBPATH%/libjpeg /I%LIBPATH%/EasyBMP /I%LIBPATH%\dcmtk\include32 /I%LIBPATH%\ndl /I%LIBPATH%\wxwidgets /I%LIBPATH%\wxwidgets\msw
SET JPEGLIB=%LIBPATH%\libjpeg\lib32\libjpeg.lib
SET BMPLIB=%LIBPATH%\EasyBMP\lib32\EasyBMP.lib
SET DICOMLIBS=%LIBPATH%\dcmtk\lib32\ofstd.lib %LIBPATH%\dcmtk\lib32\dcmdata.lib %LIBPATH%\dcmtk\lib32\dcmimgle.lib %LIBPATH%\dcmtk\lib32\dcmimage.lib netapi32.lib wsock32.lib User32.lib
SET WXLIBS=%LIBPATH%\wxwidgets\lib32\wxmsw28_core.lib %LIBPATH%\wxwidgets\lib32\wxmsw28_xrc.lib %LIBPATH%\wxwidgets\lib32\wxbase28.lib  %LIBPATH%\wxwidgets\lib32\wxmsw28_adv.lib  %LIBPATH%\wxwidgets\lib32\wxmsw28_richtext.lib  %LIBPATH%\wxwidgets\lib32\wxbase28_net.lib User32.lib Gdi32.lib shell32.lib Ole32.lib Oleaut32.lib comctl32.lib Comdlg32.lib Advapi32.lib
SET OPENMPLIB=%LIBPATH%/openmp/lib32/VCOMP.lib
SET MYLIBS=%DICOMLIBS% %WXLIBS% %OPENMPLIB% %JPEGLIB% %BMPLIB%
REM ~ SET MYLIBS=%DICOMLIBS% %WXLIBS% %JPEGLIB% %BMPLIB%
cls
rc resource.rc
%MYCPP% %MYINC% nvol.cpp /link %MYLIBS% %MYLINKFLAGS% resource.res
move *.exe %BINPATH%
move *.manifest %BINPATH%
del *.obj
