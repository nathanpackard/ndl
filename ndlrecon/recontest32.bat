cls
call b32
IF %ERRORLEVEL% NEQ 0 GOTO End

REM ~ REM FDK
REM ~ bin\ndlrecon -algorithm:1.3 -reconwidth:512 -notes:BCTrecon_FDK_shepp_logan_FIXED_NORM "C:/IM/CTA0296"
REM ~ bin\ndlrecon -algorithm:1.1 -reconwidth:512 -denoise:0.3 -notes:BCTrecon_FDK_ramp "C:/IM/CTB5766
REM ~ ..\..\bin\bctrecon -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp "C:/IM/CTB5766

REM ~ REM OSART
REM ~ ..\..\bin\bctrecon -reconwidth:512 -numiterations:1 -algorithm:3 -notes:BCTrecon_ART "C:/IM/CTB5766"

REM ~ REM PROB RECON
REM ~ ..\..\bin\bctrecon -algorithm:5.1 -reconwidth:512 -notes:BCTrecon_PROB_DENOISE_0.1 "C:/IM/CTB5766

REM ~ REM OSTV
REM ~ ..\..\bin\bctrecon -reconwidth:512 -numiterations:1 -algorithm:4 -denoise:0.3 -notes:BCTrecon_TV "C:/IM/CTB5766"

REM Back then Forward projection
REM ~ ..\..\bin\bctrecon -reconwidth:512 -algorithm:7 "C:/IM/CTB5766"

REM ~ REM Forwardprojection
REM ~ ..\..\bin\bctrecon -algorithm:6 "C:/IM/CTA0296"
REM ~ ..\..\bin\bctrecon -algorithm:6 "C:/IM/CTB5766"

REM ~ REM OSEM
REM ~ ..\..\bin\bctrecon -reconwidth:512 -numiterations:1 -algorithm:2 -notes:BCTrecon_EM "C:/IM/CTB5766"


REM WEIGHTED FDK
REM ~ ..\..\bin\bctrecon -algorithm:9.1 -reconwidth:512 -notes:BCTrecon_WEIGHTED_FDK_RAMP "C:/IM/CTB5766
REM ~ ..\..\bin\bctrecon -algorithm:9.1 -reconwidth:512 -notes:BCTrecon_WEIGHTED_FDK_RAMP "C:/IM/CTB5766

REM PROB RECON
REM ~ ..\..\bin\bctrecon -algorithm:5.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp "C:/IM/CTB5766


REM ~ REM OSART
REM ~ ..\..\bin\bctrecon -reconwidth:512 -numiterations:1 -algorithm:3 -notes:BCTrecon_ART "C:/IM/CTB5766"

REM ~ ..\..\bin\bctrecon -algorithm:8 -reconwidth:512 -notes:BCTrecon_PROB_DENOISE_1 -denoise:0.1 "C:/IM/CTB5766

bin32\ndlrecon -algorithm:1.3 -reconwidth:512 -notes:BCTrecon_FDK_shepp_logan "C:/IM/CTB5766

:End
