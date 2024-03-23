#!/bin/bash

# col,prcol,fname,field
COL=$1
PRCOL=$2
FNAME=$3
FIELD=$4

echo -n "$FIELD"|tr 'a-z' 'A-Z'
