cls
call b
IF %ERRORLEVEL% NEQ 0 GOTO End

REM ~ REM FDK PRE-CONTRAST
REM ~ ..\..\bin\bctrecon -algorithm:1.3 -reconwidth:512 -notes:BCTrecon_FDK_SheppLogan "C:/IM/CTB4591
REM ~ ..\..\bin\bctrecon -algorithm:1.3 -reconwidth:512 -notes:BCTrecon_FDK_SheppLogan "C:/IM/CTB4620
REM ~ ..\..\bin\bctrecon -algorithm:1.3 -reconwidth:512 -notes:BCTrecon_FDK_SheppLogan "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon -algorithm:1.3 -reconwidth:512 -notes:BCTrecon_FDK_SheppLogan "C:/IM/CTB4564

REM ~ REM FDK POST-CONTRAST
REM ~ ..\..\bin\bctrecon -algorithm:1.3 -reconwidth:512 -notes:BCTrecon_FDK_SheppLogan "C:/IM/CTB4592
REM ~ ..\..\bin\bctrecon -algorithm:1.3 -reconwidth:512 -notes:BCTrecon_FDK_SheppLogan "C:/IM/CTB4621
REM ~ ..\..\bin\bctrecon -algorithm:1.3 -reconwidth:512 -notes:BCTrecon_FDK_SheppLogan "C:/IM/CTB4700
REM ~ ..\..\bin\bctrecon -algorithm:1.3 -reconwidth:512 -notes:BCTrecon_FDK_SheppLogan "C:/IM/CTB4565

REM ~ REM DENOISED FDK, PRE-CONTRAST
REM ~ ..\..\bin\bctrecon -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp_TVdenoised_0.1 -denoise:0.1 "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp_TVADAPdenoised_0.3 -denoise:1.3 "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp_TVdenoised_0.2 -denoise:0.2 "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp_TVADAPdenoised_0.4 -denoise:1.4 "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp_TVdenoised_0.3 -denoise:0.3 "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp_TVADAPdenoised_0.5 -denoise:1.5 "C:/IM/CTB4699

REM ~ ..\..\bin\bctrecon -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp_TVdenoised -denoise "C:/IM/CTB4620
REM ~ ..\..\bin\bctrecon -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp_TVdenoised -denoise "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp_TVdenoised -denoise "C:/IM/CTB4564

REM ~ REM DENOISED FDK, POST-CONTRAST
REM ~ ..\..\bin\bctrecon -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp_TVdenoised -denoise "C:/IM/CTB4592
REM ~ ..\..\bin\bctrecon -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp_TVdenoised -denoise "C:/IM/CTB4621
REM ~ ..\..\bin\bctrecon -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp_TVdenoised -denoise "C:/IM/CTB4700
REM ~ ..\..\bin\bctrecon -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp_TVdenoised -denoise "C:/IM/CTB4565

REM ~ REM ART PRE-CONTRAST
REM ~ ..\..\bin\bctrecon -algorithm:3 -reconwidth:512 -deblur:1.0 -numiterations:1 -notes:BCTrecon_ART "C:/IM/CTB4699

REM ~ REM TV PRE-CONTRAST
REM ~ ..\..\bin\bctrecon -algorithm:4 -reconwidth:512 -numiterations:1 -breakoutafterframe:50 -notes:BCTrecon_TV "C:/IM/CTB4699

REM ~ REM Projection Tester
REM ~ ..\..\bin\bctrecon -reconwidth:700 -algorithm:7 -deblur:1.0 "C:/IM/CTB4699"

REM Projection Tester
REM ~ ..\..\bin\bctrecon -algorithm:6 -deblur:1.0 "C:/IM/CTB4591"

REM WEIGHTED FDK
REM ~ ..\..\bin\bctrecon -algorithm:9.1 -reconwidth:512 -notes:BCTrecon_WEIGHTED_FDK_RAMP "C:/IM/CTB5766
REM ~ ..\..\bin\bctrecon -algorithm:9.1 -reconwidth:512 -notes:BCTrecon_WEIGHTED_FDK_RAMP "C:/IM/CTB4699
REM ~ -breakoutafterframe:30

REM ~ ..\..\bin\bctrecon -algorithm:5 -reconwidth:512 -notes:BCTrecon_FDK_ramp "C:/IM/CTB4699

REM FDK SUBSET TESTING
..\..\bin\bctrecon -algorithm:10.1 -reconwidth:512 -notes:BCTrecon_FDK_MEDIAN "C:/IM/CTB4699
