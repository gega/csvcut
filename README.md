# csvcut
cut implementation for csv files

Parsing is based on [ccsv](https://github.com/gega/ccsv)

Generate makefiles with:

```
$ ./autogen.sh
$ ./configure
```

and make or make install


Argument parsing based on [BSD cut.c](https://github.com/freebsd/freebsd-src/blob/937a0055858a098027f464abf0b2b1ec5d36748f/usr.bin/cut/cut.c)

```
csvcut [-f list] [-H] [-o csv|json|xml] [-d delim] [-D output-delim] [-c field:cmd] [file ...]
```


# Description

csvcut is a cut implementation for csv files.
Parsing the input as csv formatted text file according to RFC 4180 with the exception that line
ending is based on the system default.


# Options


 **-f** _fields_  
  Select fields. This could be one range or several ranges separated by comma.
  A range is one of the following:
  
    * N  
      N'th field counting from 1
    * N-  
      N'th field and all following fields
    * N-M  
      fields between N'th and M'th including both given fields
    * **-M**  
      all fields from the first to M'th
  
 **-H**  
  Skip the first row from the output. The first row usually the header for csv
  files. This option is implicitly set when choosing an output format other
  than csv.
  
* **-d** _delim_  
  Choose a delimiter for the input csv. Default is a comma ',' character.
  
 **-D** _delim_  
  Choose a delimiter for the output csv. Default is a comma ',' character.
  This option is ignored for any output format except csv.
  
 **-o** _format_  
  Specify the output format. Default is 'csv'. Choose from the following
  formats:
   * csv  
      Default csv format
    * json  
      JSON output. Requires csv input with header row. The **-H** option is implicitly set
    * xml  
      XML output. Requires csv input with header row. The **-H** option is implicitly set.
  
 **-c** _field:command_  
  For the specified field, calls the given command and the standard output
  from the command will be placed instead of the field actual value. Can be
  used multiple times for different fields. The
  passed arguments to the command:
   
    * 1  
      column number in the input file
    * 2  
      column number in the output file
    * 3  
      name of the field, according to the header row
    * 4  
      actual field value
  
 **-h**  
  Summary of command line arguments and exit
  
