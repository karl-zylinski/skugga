call "%VS140COMNTOOLS%..\\..\\VC\\vcvarsall.bat" amd64
cl.exe /D _HAS_EXCEPTIONS=0 /W4 /TP /DUNICODE /wd4201 /wd4100 /D _CRT_SECURE_NO_WARNINGS /Zi /MTd /D DEBUG memory_test.cpp /link /subsystem:windows /entry:mainCRTStartup /out:test_memory.exe