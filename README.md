# csvcut
cut implementation for csv files

Parsing is based on [ccsv](https://github.com/gega/ccsv)

Argument parsing based on [BSD cut.c](https://github.com/freebsd/freebsd-src/blob/937a0055858a098027f464abf0b2b1ec5d36748f/usr.bin/cut/cut.c)

usage: csvcut -f list [-H] [-J] [-d delim] [file ...]

  -f list
  
   The list	specifies fields, separated in the input by the	field delimiter character (see  the	-d option). Output fields are separated by a single occurrence of the field delimiter character. Fields are interpreted according to RFC 4180 with the exception of the line ending character is based on the system default, for Unix based systems '\n'
  
  -H
  
   Skip the first row which usually contains the header information
  
  -d delim
  
   Use _delim_ as the field	delimiter character instead of the comma character.
  
  -J

   Generate JSON output. This flag implies the -H flag.
