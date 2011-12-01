#!/bin/bash 

# this file is intended to be used with cron -- it kills the output 
# so cron doesn't send you an email every time the script runs

#python -W ignore::DeprecationWarning ./poll_gmail.py &> /dev/null
export PATH=$PATH:$HOME/marss.utils 
poll_gmail.py &> /dev/null
