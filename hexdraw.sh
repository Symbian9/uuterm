#!/bin/sh
# Script to mimic the encoding half of Roman Czyborra's hexdraw
# script using only tr and sed. Should be 100% portable in both
# the POSIX sense and the legacy Unix sense.

tr '[\n ]' '[ \n]' \
| sed -e 's/	\([^ ]*\) /\1/g' \
| tr '[\n ]' '[ \n]' \
| sed -e '/^:/!b x
s/:/:@/
: r
s/@----/0@/
t r
s/@---#/1@/
t r
s/@--#-/2@/
t r
s/@--##/3@/
t r
s/@-#--/4@/
t r
s/@-#-#/5@/
t r
s/@-##-/6@/
t r
s/@-###/7@/
t r
s/@#---/8@/
t r
s/@#--#/9@/
t r
s/@#-#-/A@/
t r
s/@#-##/B@/
t r
s/@##--/C@/
t r
s/@##-#/D@/
t r
s/@###-/E@/
t r
s/@####/F@/
t r
s/@//
: x'
