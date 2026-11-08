# Writes this build's stamp, <year>.<month><day>.<hours><minutes><seconds>, for the launcher's footer
# and the zip's name, run by the LTCallisto_Stamp target on every build (cmake -P).
#
#   STAMP_CPP  the source the launchers compile
#   STAMP_TXT  the same stamp alone, which package_zip.cmake reads
#
# The LTCALLISTO_STAMP environment variable - when it holds a stamp - is used instead of the time,
# so a Linux build can carry the stamp of the Windows build it goes with

set(stamp "$ENV{LTCALLISTO_STAMP}")
if(NOT stamp MATCHES "^[0-9][0-9][0-9][0-9]\\.[0-9][0-9][0-9][0-9]\\.[0-9][0-9][0-9][0-9][0-9][0-9]$")
    if(stamp)
        message(WARNING "LTCALLISTO_STAMP '${stamp}' isn't a <year>.<month><day>.<time> stamp, so the time is used")
    endif()
    string(TIMESTAMP stamp "%Y.%m%d.%H%M%S")
endif()
file(WRITE "${STAMP_CPP}" "// Written by tools/launcher/stamp.cmake on every build\nconst char *g_szLauncherBuild = \"${stamp}\";\n")
file(WRITE "${STAMP_TXT}" "${stamp}")
