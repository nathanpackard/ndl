SET LIBPATH=..\..\..
SET WXLIBS=%LIBPATH%\wxwidgets\lib32\wxmsw28_core.lib %LIBPATH%\wxwidgets\lib32\wxmsw28_xrc.lib %LIBPATH%\wxwidgets\lib32\wxbase28.lib  %LIBPATH%\wxwidgets\lib32\wxmsw28_adv.lib  %LIBPATH%\wxwidgets\lib32\wxmsw28_richtext.lib  %LIBPATH%\wxwidgets\lib32\wxbase28_net.lib User32.lib Gdi32.lib shell32.lib Ole32.lib Oleaut32.lib comctl32.lib Comdlg32.lib Advapi32.lib

cls
cl /EHsc /DWIN32 /MD /I%LIBPATH%\wxwidgets /I%LIBPATH%\wxwidgets\msw test.cpp /link %WXLIBS% user32.lib gdi32.lib shell32.lib ole32.lib comctl32.lib wsock32.lib 
test