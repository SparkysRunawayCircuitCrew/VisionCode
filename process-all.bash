#!/bin/bash
#
#  Runs every PNG file found under the current directory through the filter

declare topDir="${1:-.}";
shift;

declare -i found=0;
declare -i missed=0;
declare missing_files="";

is_output_file() {
  # If no "-" in file name then OK
  if [ "${f//-/}" == "${f}" ]; then
    return 1;
  fi

  # If it has -step somewhere in the name, assume it is an output file
  if [ "${f//-step/}" != "${f}" ]; then
    return 0;
  fi

  # Specific name checks
  for e in "-blurred" "-bw" "-contours" "-cropped" "-dialate" "-erode" \
           "-hsv" "-orig.png" "-polygons" \
           "-red.png" "-yellow.png"; do
    if [ "${f//${e}/}" != "${f}" ]; then
      # File name contained one of the common output names, consider as output file
      return 0;
    fi
  done

  return 1;
}

for f in $(find ${topDir} -name "*.png" | sort); do
  if is_output_file "${f}"; then
    continue;
  fi
  echo -e "\nProcessing: ${f}  ";
  if ./avc-vision -f ${f} "${@}"; then
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
