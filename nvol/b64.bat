SET LIBPATH=..\..\lib
SET BINPATH=bin64
REM ~ SET MYCPP=cl /Ox /EHsc /D_WIN32_WINNT=0x0400 /MD /openmp /DNDL_USE_OMP
SET MYCPP=cl /Ox /EHsc /D_WIN32_WINNT=0x0400 /MD
SET MYLINKFLAGS=/NODEFAULTLIB:LIBCMT,libc
SET MYINC=/I%LIBPATH%\bctlib /I%LIBPATH%\analyzelib /I%LIBPATH%/libjpeg /I%LIBPATH%/EasyBMP /I%LIBPATH%\dcmtk\include64 /I%LIBPATH%\ndl /I%LIBPATH%\wxwidgets /I%LIBPATH%\wxwidgets\msw
SET JPEGLIB=%LIBPATH%\libjpeg\lib64\libjpeg.lib
SET BMPLIB=%LIBPATH%\EasyBMP\lib64\EasyBMP.lib
SET DICOMLIBS=%LIBPATH%\dcmtk\lib64\ofstd.lib %LIBPATH%\dcmtk\lib64\dcmdata.lib %LIBPATH%\dcmtk\lib64\dcmimgle.lib %LIBPATH%\dcmtk\lib64\dcmimage.lib netapi32.lib wsock32.lib User32.lib
SET WXLIBS=%LIBPATH%\wxwidgets\lib64\wxmsw28_core.lib %LIBPATH%\wxwidgets\lib64\wxmsw28_xrc.lib %LIBPATH%\wxwidgets\lib64\wxbase28.lib  %LIBPATH%\wxwidgets\lib64\wxmsw28_adv.lib  %LIBPATH%\wxwidgets\lib64\wxmsw28_richtext.lib  %LIBPATH%\wxwidgets\lib64\wxbase28_net.lib User32.lib Gdi32.lib shell32.lib Ole32.lib Oleaut32.lib comctl32.lib Comdlg32.lib Advapi32.lib
REM ~ SET OPENMPLIB=%LIBPATH%/openmp/lib64/VCOMP.lib
REM ~ SET MYLIBS=%DICOMLIBS% %WXLIBS% %OPENMPLIB% %JPEGLIB% %BMPLIB%
SET MYLIBS=%DICOMLIBS% %WXLIBS% %JPEGLIB% %BMPLIB%
cls
%MYCPP% %MYINC% nvol.cpp /link %MYLIBS% %MYLINKFLAGS%
move *.exe %BINPATH%
move *.manifest %BINPATH%
del *.obj
