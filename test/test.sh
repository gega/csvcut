#!/bin/bash

# sample files from:
# https://www.datablist.com/learn/csv/download-sample-csv-files
# https://www.epa.gov/sites/default/files/2016-01/ncca_qa_codes.csv
# https://go.microsoft.com/fwlink/?LinkID=521962

WHERE=$(dirname "$0")

testno=0
valgrind=0
output_file=""

tests=(
    "-H -f4-5,-2,8- $WHERE/organizations-100.csv"
    "-o json -f4-5,-2,8- $WHERE/organizations-100.csv"
    "$WHERE/customers-100.csv"
    "-o json -f1,4 $WHERE/ncca_qa_codes.csv"
    "-o json -f12-,1 -d ';' $WHERE/FinancialSample.csv"
    "-o xml $WHERE/customers-100.csv"
    "-H -f 2-4 -c 2:$WHERE/procfield $WHERE/customers-100.csv"
)

hash=(
    "6175720a6ae45c47d5e4843fec16d7f6"
    "06cafa07b2393e688f3d038f4e07e4b1"
    "2982e2838f38fa8043cdc440a6efbadc"
    "c5bac232bfc3ecb3678bdc7291fa1a42"
    "2f3728c78254f3087d74b2bd1503eae4"
    "54284c7f40073a427b582f54bd46fc86"
    "21030d2c89b6ba6ebac76d8b9e3bd765"
)

function show_help()
{
  echo "Usage $(basename $0) "
  exit 0
}

while getopts "h?vo:t:T" opt; do
  case "$opt" in
    h|\?)
      show_help
      exit 0
      ;;
    v)  valgrind=1
      ;;
    t)  testno=$OPTARG
      ;;
    T)  echo "${#tests[@]}"
        exit 0
      ;;
    o)  output_file=$OPTARG
      ;;
  esac
done

if [[ "$(basename $0)" == *"_T"* ]]; then
  testno=$(echo "$(basename $0 .sh)" | sed -e 's/.*_T//g')
fi

if [ $testno -lt 1 ]; then
  exit 1
fi

testno=$((testno-1))

RESP=0

TMP=$(mktemp)
XML=$(mktemp)

if [ x"$output_file" != x"" ]; then
  echo -e "----------------------------------------\n$(date)\nRunning test #${testno}" >>$output_file
fi
if [ $valgrind -ne 0 ]; then
  HASH=$(echo "valgrind --xml=yes --xml-file=$XML $WHERE/../src/csvcut ${tests[$testno]} | md5sum | cut -d' ' -f1" | sh 2>$TMP )
else
  HASH=$(echo "$WHERE/../src/csvcut ${tests[$testno]} | md5sum | cut -d' ' -f1" | sh 2>$TMP )
fi
FAIL=0
if [ x"${hash[$testno]}" != x"$HASH" ]; then
  if [ x"$output_file" != x"" ]; then
    echo "  test #${tst} FAILED" >>$output_file
    echo "  output: $(cat $TMP)" >>$output_file
    echo "  diff:" >>$output_file
    echo "$WHERE/../src/csvcut ${tests[$testno]} | diff $WHERE/tout/T${testno}.tout -" | sh >>$output_file
  fi
  RESP=1
  FAIL=1
fi
if [ $valgrind -ne 0 ]; then
  ER=$(cat $XML|awk 'BEGIN {e=0} /errorcounts/ { e=1-e; } // {if(e!=0) print $0;}'|wc -l)
  if [ $ER -gt 1 ]; then
    if [ x"$output_file" != x"" ]; then
      echo "test #$testno FAILED [valgrind error]" >>$output_file
      echo "$WHERE/../src/csvcut ${tests[$testno]}" >>$output_file
      cat $XML >>$output_file
    fi
    RESP=1
    FAIL=2
  fi
fi
if [ $FAIL -eq 0 ]; then
  if [ x"$output_file" != x"" ]; then
    echo "PASS: case #$testno" >>$output_file
  fi
fi

rm -f $TMP
rm -f $XML

exit $RESP
