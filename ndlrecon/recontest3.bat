cls
call b64
IF %ERRORLEVEL% NEQ 0 GOTO End

REM ~ REM FDK PRE-CONTRAST
REM ~ ..\..\bin\bctrecon64 -algorithm:1.3 -reconwidth:256 -notes:BCTrecon_FDK_SheppLogan_256 "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon64 -algorithm:1.3 -reconwidth:512 -notes:BCTrecon_FDK_SheppLogan_512 "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon64 -algorithm:1.3 -reconwidth:700 -notes:BCTrecon_FDK_SheppLogan_700 "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon64 -algorithm:1.3 -reconwidth:800 -notes:BCTrecon_FDK_SheppLogan_800 "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon64 -algorithm:1.3 -reconwidth:1024 -notes:BCTrecon_FDK_SheppLogan_1024 "C:/IM/CTB4699

REM ~ REM DENOISED FDK, PRE-CONTRAST
REM ~ ..\..\bin\bctrecon64 -algorithm:1.1 -reconwidth:256 -notes:BCTrecon_FDK_ramp_TVdenoised_256 -denoise "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon64 -algorithm:1.1 -reconwidth:512 -notes:BCTrecon_FDK_ramp_TVdenoised_512 -denoise "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon64 -algorithm:1.1 -reconwidth:700 -notes:BCTrecon_FDK_ramp_TVdenoised_700 -denoise "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon64 -algorithm:1.1 -reconwidth:800 -notes:BCTrecon_FDK_ramp_TVdenoised_800 -denoise "C:/IM/CTB4699
REM ~ ..\..\bin\bctrecon64 -algorithm:1.1 -reconwidth:1024 -notes:BCTrecon_FDK_ramp_TVdenoised_1024 -denoise "C:/IM/CTB4699

REM ~ ..\..\bin\bctrecon64 -reconwidth:2048 -algorithm:7 "C:/IM/CTB4699"

REM ~ ..\..\bin\bctrecon64 -algorithm:6 "C:/IM/CTB4699"

..\..\bin\bctrecon64 -algorithm:1.1 -reconwidth:800 -notes:BCTrecon_FDK_ramp_TVdenoised_0.1 -denoise:0.1 "C:/IM/CTB4699
..\..\bin\bctrecon64 -algorithm:1.1 -reconwidth:800 -notes:BCTrecon_FDK_ramp_TVADAPdenoised_0.3 -denoise:1.3 "C:/IM/CTB4699
..\..\bin\bctrecon64 -algorithm:1.1 -reconwidth:800 -notes:BCTrecon_FDK_ramp_TVdenoised_0.2 -denoise:0.2 "C:/IM/CTB4699
..\..\bin\bctrecon64 -algorithm:1.1 -reconwidth:800 -notes:BCTrecon_FDK_ramp_TVADAPdenoised_0.4 -denoise:1.4 "C:/IM/CTB4699
..\..\bin\bctrecon64 -algorithm:1.1 -reconwidth:800 -notes:BCTrecon_FDK_ramp_TVdenoised_0.3 -denoise:0.3 "C:/IM/CTB4699
..\..\bin\bctrecon64 -algorithm:1.1 -reconwidth:800 -notes:BCTrecon_FDK_ramp_TVADAPdenoised_0.5 -denoise:1.5 "C:/IM/CTB4699


:End
