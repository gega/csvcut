#!/bin/bash

# sample files from:
# https://www.datablist.com/learn/csv/download-sample-csv-files
# https://www.epa.gov/sites/default/files/2016-01/ncca_qa_codes.csv
# https://go.microsoft.com/fwlink/?LinkID=521962

STYLE=$1
WHERE=$(dirname "$0")

if [ x"$STYLE" == x"" ]; then
  STYLE=auto
fi

tests=(
    "-H -f4-5,-2,8- $WHERE/organizations-100.csv"
    "-o json -f4-5,-2,8- $WHERE/organizations-100.csv|jq ."
    "$WHERE/customers-100.csv"
    "-o json -f1,4 $WHERE/ncca_qa_codes.csv|jq ."
    "-o json -f12-,1 -d ';' $WHERE/FinancialSample.csv|jq ."
    "-o xml $WHERE/customers-100.csv|xmllint --format -"
    "-H -f 2-4 $WHERE/customers-100.csv -c 2:$WHERE/procfield.sh"
)

hash=(
    "6175720a6ae45c47d5e4843fec16d7f6"
    "8f3fe0123bd61f002b9f89e74213ad05"
    "2982e2838f38fa8043cdc440a6efbadc"
    "38ae0da7c1bcd42b90678d6b332222b5"
    "8ac35e68a3e32d675ef2c1e1ec00bb2f"
    "3d27e976a63b5a4d03c0273323a3b058"
    "21030d2c89b6ba6ebac76d8b9e3bd765"
)

RESP=0

TMP=$(mktemp)
XML=$(mktemp)

for (( i=0; i<${#tests[@]}; i++ ));
do
  HASH=$(echo "valgrind --xml=yes --xml-file=$XML $WHERE/../src/csvcut ${tests[$i]} | md5sum | cut -d' ' -f1" | sh 2>$TMP )
  FAIL=0
  if [ x"${hash[$i]}" != x"$HASH" ]; then
    if [ x"$STYLE" == x"auto" ]; then
      echo "FAIL: test #$i"
    else
      echo "test #$i FAILED [stderr: $(cat $TMP)]"
      echo "$WHERE/../src/csvcut ${tests[$i]} | diff $WHERE/tout/T${i}.tout -" | sh
    fi
    RESP=1
    FAIL=1
  fi
  ER=$(cat $XML|awk 'BEGIN {e=0} /errorcounts/ { e=1-e; } // {if(e!=0) print $0;}'|wc -l)
  if [ $ER -gt 1 ]; then
    if [ x"$STYLE" == x"auto" ]; then
      echo "FAIL: mem  #$i"
    else
      echo "test #$i FAILED [valgrind error]"
      echo "$WHERE/../src/csvcut ${tests[$i]}"
      cat $XML
    fi
    RESP=1
    FAIL=2
  fi
  if [ $FAIL -eq 0 ]; then
    if [ x"$STYLE" == x"auto" ]; then
      echo "PASS: case #$i"
    fi
  fi
done

rm -f $TMP
rm -f $XML

exit $RESP
