#!/bin/bash

# sample files from:
# https://www.datablist.com/learn/csv/download-sample-csv-files
# https://www.epa.gov/sites/default/files/2016-01/ncca_qa_codes.csv
# https://go.microsoft.com/fwlink/?LinkID=521962

tests=(
    "-H -f4-5,-2,8- organizations-100.csv"
    "-o json -f4-5,-2,8- organizations-100.csv|jq ."
    "customers-100.csv"
    "-o json -f1,4 ncca_qa_codes.csv|jq ."
    "-o json -f12-,1 -d ';' FinancialSample.csv|jq ."
    "-o xml customers-100.csv|xmllint --format -"
    "-H -f 2-4 customers-100.csv -c 2:./procfield.sh"
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

TMP=$(mktemp)

for (( i=0; i<${#tests[@]}; i++ ));
do
  HASH=$(echo "../src/csvcut ${tests[$i]} | md5sum | cut -d' ' -f1" | sh 2>$TMP )
  if [ x"${hash[$i]}" != x"$HASH" ]; then
    echo "test #$i FAILED [stderr: $(cat $TMP)]"
    echo "../src/csvcut ${tests[$i]} | diff tout/T${i}.tout -" | sh
  fi
done

rm -f $TMP
