#! /usr/bin/env bash


source_dir="./src"
object_dir="./Coverage/CMakeFiles/secure_socket.dir/src"

sources="$(ls $source_dir/*.c)"

for s in $sources; do
    echo "# gcov on $s"
    gcov "$s" -b -f -l -u -o "$object_dir/$(basename $s).gcno"
done

mkdir gcov
mv *.gcov gcov/