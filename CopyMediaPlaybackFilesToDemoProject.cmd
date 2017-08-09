@echo off
xcopy "%~dp0\MediaPlayback\Unity\MediaPlayback\Assets\*.dll" "%~dp0\MediaPlaybackDemo\Assets\MediaPlayback\" /E /C /H /R /Y
xcopy "%~dp0\MediaPlayback\Unity\MediaPlayback\Assets\*.pdb" "%~dp0\MediaPlaybackDemo\Assets\MediaPlayback\" /E /C /H /R /Y