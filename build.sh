#!/usr/bin/env sh

set -e
#set -ex

# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2017-2018 Bytemare <d@bytema.re>. All Rights Reserved.

#
# Parameter gathering and building out-of-source with CMAKE
#


# Check if file exists
check_file()
{
    [ -f "$1" ] || ( printf "\n[Error] %s does not exist !" "$1" && exit 1)
}

printf "Checking for build files ..."

PARAMETERS="./parameters.conf" && check_file ${PARAMETERS}
CMAKELISTS="./CMakeLists.txt" && check_file ${CMAKELISTS}

printf " done.\n"

#
# Source parameter file
#

# shellcheck source=parameters.conf
. ${PARAMETERS}


# Targets to be created and deleted
BUILDS="$RELEASE $DEBUG $COVERAGE"
CREAT="$BUILDS $LINK $RUNNER $SOCKET_PATH $CLEANER"


# Check if requested build exists. If not, default to Debug
BUILD_TYPE="$DEBUG"
if [ "$#" -ne 0 ]; then
    if test "${BUILDS#*$1}" = "$BUILDS"; then
        echo "Invalid argument !"
        printf '[ERROR] Invalid build type "%s". Please specify a build type among : %s.\n\n' "$1" "$BUILDS"
        exit 1
    else
        BUILD_TYPE="$1"
        printf '[INFO] Building %s\n' "$BUILD_TYPE"
    fi
else
    printf '[INFO] Building %s (default build)\n' "$BUILD_TYPE"
fi


# Clean up previous work
if [ "$#" -ne 0 && "$2" = "clean" ]; then
    s="rm --force -rf $CREAT"
    $s
fi

# Build cleaner script
echo "$SHELL_H" > "$CLEANER"
echo "$s" >> "$CLEANER"
chmod 700 "$CLEANER"

# Build project by calling cmake
# arg1 : name of the directory to build the project in
# arg2 : arguments to give to cmake
build(){
    build_directory="$1"
    options="$2"
    curr_dir=$(pwd)

    mkdir -p "$build_directory"
    # Use a subshell in case cd doesn't work, and don't mess with the directories
    (
        cd "$build_directory" || ( printf "[ERROR] Could not move into %s\n" "$build_directory" && exit 1 )
        cmake "$options" "$curr_dir"
        make
        s="./$build_directory/$LINK "
        cd "$curr_dir"
    )
}

# Build
# -D CMAKE_C_COMPILER=/usr/bin/gcc
build "$BUILD_TYPE" "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"


#ln -s ./$BUILD/$EXEC $LINK
#chmod -R 500 *
#chown -R secure_socket *
#chgrp -R secure_socket *


# Build launching script
s="./$BUILD_TYPE/$LINK"
s=$s" socket_path=$SOCKET_PATH"
s=$s" mq_name=$MQ_NAME"
s=$s" log_file=$LOG_FILE"
s=$s" domain=$DOMAIN"
s=$s" protocol=$PROTOCOL"
s=$s" max_connections=$MAX_CONNECTIONS"
s=$s" socket_permissions=$SOCKET_PERMISSIONS"
s=$s" authorised_peer_username=$AUTHORISED_PEER_NAME"
s=$s" verbosity=$VERBOSITY"



echo "$SHELL_H" > "$RUNNER"
echo "$s" >> "$RUNNER"
chmod 700 "$RUNNER"
