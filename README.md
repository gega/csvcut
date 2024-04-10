# csvcut
cut implementation for csv files

Parsing is based on [ccsv](https://github.com/gega/ccsv)

Generate makefiles with:

```
$ ./autogen.sh
$ ./configure
```

and make, make check and make install


Argument parsing based on [BSD cut.c](https://github.com/freebsd/freebsd-src/blob/937a0055858a098027f464abf0b2b1ec5d36748f/usr.bin/cut/cut.c)

```
csvcut [-f list] [-H] [-o csv|json|xml] [-d delim] [-D output-delim] [-c field/args:cmd] [file ...]
```

# Description

csvcut is a cut implementation for csv files.
Parsing the input as csv formatted text file according to RFC 4180 with the exception that line
ending is based on the system default.

## OPTIONS

**−f** _fields_

Select fields. This could be one range or several ranges separated by comma. A range is one of the following:

|     |     |     |     |
| --- | --- | --- | --- |
| N   |     | N’th field counting from 1 |     |
| N−  |     | N’th field and all following fields |     |
| N−M |     | fields between N’th and M’th including both given fields |     |
| **−M** |     | all fields from the first to M’th |     |
| **−H** |     |     |     |

Skip the first row from the output. The first row usually the header for csv files. This option is implicitly set when choosing an output format other than csv.

**−d** _delim_

Choose a delimiter for the input csv. Default is a comma ’,’ character.

**−D** _delim_

Choose a delimiter for the output csv. Default is a comma ’,’ character. This option is ignored for any output format except csv.

**−o** _format_

Specify the output format. Default is ’csv’. Choose from the following formats:

|     |     |     |
| --- | --- | --- |
| csv |     | Default csv format |
| json |     | JSON output. Requires csv input with header row. The **−H** option is implicitly set |
| xml |     | XML output. Requires csv input with header row. The **−H** option is implicitly set. |

**−c** _field:command_

For the specified field, calls the given command and the standard output from the command will be placed instead of the field actual value. Two ranges can be specified, separated by a forward slash ’/’ character. The first range should be one field or a single range and selects the fields needs top be rewritten. The second rangeset may contain multiple ranges separated by comma ’,’ character and the selected fields are passed to the processing command as extra arguments. Can be used multiple times for different fields. The passed arguments to the command:

|     |     |     |     |
| --- | --- | --- | --- |
| 1   |     | column number in the input file |     |
| 2   |     | column number in the output file |     |
| 3   |     | name of the field, according to the header row |     |
| 4   |     | actual field value |     |
| 5+  |     | extra arguments specified by the second range set |     |

**−h**

Summary of command line arguments and exit

## EXAMPLES

```
$ csvcut -H -f 1-3,5,9- -c 2-3/6,8-9:pf\_1 -c 5/2,4:pf\_2 input.csv
```
This command will process input.csv, outputting 1,2,3,5,9 and all the rest of the columns with processing of 2nd, 3rd and 5th fields, omitting the first row. The columns 2 and 3 will be processed by an executable called pf\_1 which will receive the following arguments when called for the column 3:

|     |     |     |     |     |
| --- | --- | --- | --- | --- |
|     | 1   |     | column number in input, value 3 |     |
|     | 2   |     | column number in output, value 3 |     |
|     | 3   |     | name of the field, the 3rd field of the first row of the input |     |
|     | 4   |     | the value of the 3rd field of the current row |     |
|     | 5   |     | the value of the 6th field of the current row |     |
|     | 6   |     | the value of the 8th field of the current row |     |
|     | 7   |     | the value of the 9th field of the current row |     |

The standard output will be used as the column 3 of the generated output. The column 5 will be processed by the executable called pf\_2 and the extra arguments will be the current values of the second and forth columns.
