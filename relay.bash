#!/bin/bash
mesg y
if [ -z $1 ] 
 then

 echo "Relay command processor for Linux v3.3"
 echo "RELAY services available:
 
    TOP             STATS          UNAME             INFO
    WEATHER|city    NEWS bbc|npr   NEWTON|expression
    FORECAST|city   STOCK|symbol   MOON              PI
    DICTIONARY|word THESAURUS|word CONFERENCE|general/relay
    MOVIE|name      FORTUNE        DRINKS            GAMMA
    VMCOM           MAZE           QUEEN             MANDELBROT
    SYSOP           UPGRADE        **ROUTE**         MATRIX
    MERSENNE        AMICABLE NUMBERS
    FACTORS         SUDOKU
    ----------------------------------------------------------------
 
-> Please provide a command following the RELAY invocation"

else 
  echo "Relay command processor v3.3 args: "$1$2$3
  echo "Please wait 3-4 seconds for output.."
  /usr/local/bin/send -c @RELAY $1 $2 $3 2>&1
fi
