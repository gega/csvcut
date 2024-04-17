#!/bin/bash

# sample files from:
# https://www.datablist.com/learn/csv/download-sample-csv-files
# https://www.epa.gov/sites/default/files/2016-01/ncca_qa_codes.csv
# https://go.microsoft.com/fwlink/?LinkID=521962

WHERE=$(dirname "$0")

testno=0
valgrind=0
output_file=""
all=0

tests=(
    "-H -f4-5,-2,8- $WHERE/organizations-100.csv"		#1
    "-o json -f4-5,-2,8- $WHERE/organizations-100.csv"		#2
    "$WHERE/customers-100.csv"					#3
    "-o json -f1,4 $WHERE/ncca_qa_codes.csv"			#4
    "-o json -f12-,1 -d ';' $WHERE/FinancialSample.csv"		#5
    "-o xml $WHERE/customers-100.csv"				#6
    "-H -f 2-4 -c 3:$WHERE/procfield $WHERE/customers-100.csv"	#7
    "-H -f 2-4 -c 2-4:$WHERE/procfield customers-100.csv"	#8
    "-H -f 3-4,11 -c 3-4/11:$WHERE/procfield customers-100.csv"	#9
    "-H -c 4/1:$WHERE/procfield customers-100.csv"		#10
    "-r 7-,1,3-4,-2 -H $WHERE/organizations-100.csv"		#11
)

hash=(
    "6175720a6ae45c47d5e4843fec16d7f6"  # 1
    "4daf77c78eec8ab89db9476df4d1ae33"  # 2
    "2982e2838f38fa8043cdc440a6efbadc"  # 3
    "8da51b6a13bb8cc227313b3d5e9e8527"  # 4
    "7e00072b9e7d37e6786b0e3c52b90704"  # 5
    "126c04364792c4c2f63c922c1258c0ed"  # 6
    "21030d2c89b6ba6ebac76d8b9e3bd765"  # 7
    "695793fb53972160ec1f87d62bd47aab"  # 8
    "f2ed118126712ed0c71b643d93c207f1"  # 9
    "a5069b1a2dfd3789b5545a02fc4d8816"  # 10
    "110b56cf242e43a6995b6ea645e4f822"  # 11
)

function show_help()
{
  echo "Usage $(basename $0)"
  echo -e "\t-h\t\thelp"
  echo -e "\t-v\t\tuse valgrind during tests"
  echo -e "\t-a\t\trun all tests"
  echo -e "\t-T\t\tprint number of tests"
  echo -e "\t-t N\t\trun test number #N"
  echo -e "\t-o file\t\toutput filename"
  exit 0
}

while getopts "h?vo:t:Ta" opt; do
  case "$opt" in
    h|\?)
      show_help
      exit 0
      ;;
    v)  valgrind=1
      ;;
    a)  all=1
        testno=1
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

maxtest=$((testno+1))
if [ $all -eq 1 ]; then
  maxtest=${#tests[@]}
fi

if [ x"$output_file" == x"" ]; then
  exec > /dev/null
elif [ x"$output_file" != x"-" ]; then
  exec >> $output_file
fi

for((i=$testno;i<$maxtest;i++))
do
  testno=$i
  thrn=$((testno+1))
  if [ ! -f "$WHERE/test_T${thrn}.sh" ]; then
    ln -s "$WHERE/test.sh" "$WHERE/test_T${thrn}.sh"
  fi
  if [ x"$output_file" != x"" ]; then
    echo -e "----------------------------------------\n$(date)\nRunning test #${thrn}"
  fi
  if [ $valgrind -ne 0 ]; then
    HASH=$(echo "valgrind --xml=yes --xml-file=$XML $WHERE/../src/csvcut ${tests[$testno]} | md5sum | cut -d' ' -f1" | sh 2>$TMP )
  else
    HASH=$(echo "$WHERE/../src/csvcut ${tests[$testno]} | md5sum | cut -d' ' -f1" | sh 2>$TMP )
  fi
  FAIL=0
  if [ x"${hash[$testno]}" != x"$HASH" ]; then
    if [ x"$output_file" != x"" ]; then
      echo "  test #${thrn} FAILED"
      echo "  output: $(cat $TMP)"
      echo "  diff:"
      echo "$WHERE/../src/csvcut ${tests[$testno]} | diff $WHERE/tout/T${thrn}.tout -" | sh
    fi
    RESP=1
    FAIL=1
  fi
  if [ $valgrind -ne 0 ]; then
    ER=$(cat $XML|awk 'BEGIN {e=0} /errorcounts/ { e=1-e; } // {if(e!=0) print $0;}'|wc -l)
    if [ $ER -gt 1 ]; then
      if [ x"$output_file" != x"" ]; then
        echo "test #$thrn FAILED [valgrind error]"
        echo "$WHERE/../src/csvcut ${tests[$testno]}"
        cat "$XML"
      fi
      RESP=1
      FAIL=2
    fi
  fi
  if [ $FAIL -eq 0 ]; then
    if [ x"$output_file" != x"" ]; then
      echo "PASS: case #$thrn"
    fi
  fi
done

rm -f $TMP
rm -f $XML

exit $RESP
