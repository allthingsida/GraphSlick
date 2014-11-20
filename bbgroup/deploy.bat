@echo off

set DEST=p:\tools\idadev\plugins\GraphSlick

FOR %%a in (bb_utils bb_ida bb_types bb_match bb_match_final) DO copy %%a.py %DEST%

copy bb_match_final.py p:\tools\idadev\plugins\GraphSlick\bb_match.py