#!/bin/bash
#
#  Runs every PNG file found under the current directory through the filter

declare -i found=0;
declare -i missed=0;
declare missing_files="";

for f in $(find . -name "*.png"); do
  echo -e "\nProcessing: ${f}  ";
  if ./vision -f ${f}; then
    found=$((found + 1));
  else
    missed=$((missed + 1));
    missing_files="${missing_files} $f";
  fi
done

declare -i total=$((found + missed));

echo "

Found stanchion in ${found} of the ${total} images (missed ${missed})
"

for f in ${missing_files}; do
    echo "  $f";
done

exit ${missed};
