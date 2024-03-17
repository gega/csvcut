# csvcut
cut implementation for csv files

Parsing is based on [ccsv](https://github.com/gega/ccsv)

Argument parsing based on [BSD cut.c](https://github.com/freebsd/freebsd-src/blob/937a0055858a098027f464abf0b2b1ec5d36748f/usr.bin/cut/cut.c)

usage: ./csvcut -f list [-H] [-d delim] [file ...]

  -f list
  
   The list	specifies fields, separated in the input by the	 field delimiter  character  (see  the	-d option).  Output fields are separated by a single occurrence	of the field delimiter character.
  
  -H
  
   Skip the first row which usually contains the field names only
  
  -d delim
  
   Use _delim_ as the field	delimiter character instead of the comma character.
  
