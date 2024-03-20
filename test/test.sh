#!/bin/bash

# sample files from:
# https://www.datablist.com/learn/csv/download-sample-csv-files
# https://www.epa.gov/sites/default/files/2016-01/ncca_qa_codes.csv
# https://go.microsoft.com/fwlink/?LinkID=521962

tests=(
    "-H -f4-5,-2,8- organizations-100.csv |md5sum"
    "-J -f4-5,-2,8- organizations-100.csv|jq -c .|md5sum"
    "customers-100.csv |md5sum"
    "-J -f1,4 ncca_qa_codes.csv|jq -c .|md5sum"
    "-J -f12-,1 -d ';' FinancialSample.csv|jq -c .|md5sum"
)

TMP=$(mktemp)

for (( i=0; i<${#tests[@]}; i++ ));
do
  echo "../csvcut ${tests[$i]} >$TMP" | sh &>/dev/null
  cmp T$i.tout $TMP &>/dev/null
  if [ $? -ne 0 ]; then
    echo "test #$i FAILED"
  fi
done

rm -f $TMP
