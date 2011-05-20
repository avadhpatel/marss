#!/usr/bin/python 

# this script will generate send an email to notify you when a simulation
# finishes. It is meant to be used by adding 
#  -execute-after-kill "python util/send_gmail.py" 
# to your simconfig file. 
#
# To use this script, please follow the instructions below (using the sample
# xoauth.py script from google's examples to generate the credentials) and
# recording the data into the xoauth.txt file


import smtplib
import base64
from xoauth import * 
import random
import os
import socket

if not os.path.exists("xoauth.txt") and not os.path.exists("util/xoauth.txt"):
    print """
        Please generate a xoauth.txt. To do so, first run: 
            ./xoauth.py --generate_oauth_token --user=YOUR_USERNAME@gmail.com

        Follow the instructions until you get a oauth token and oauth token secret. 

        When you have these two values, create a xoauth.txt file with the following structure (one per line): 
            YOUR_USERNAME@gmail.com
            DESITNATION_EMAIL@whatever.com
            oauth_token
            oauth_secret

        Then re-run the sender script.
        Save this xoauth.txt file either in top Marss directory or in $MARSS/util.
    """
    exit();

if os.path.exists("xoauth.txt"):
    xoauth_txt = "xoauth.txt"
elif os.path.exists("util/xoauth.txt"):
    xoauth_txt = "util/xoauth.txt"

xoauth_cred = open(xoauth_txt)
xoauth_fields = file.readlines(xoauth_cred); 

hostname = socket.gethostname()
if len(sys.argv) == 2:
    subject_string = "MARSS Simulation %d finished on host %s" % (int(sys.argv[1]), hostname)
    logfile_name = "../run%d.log" % ( int(sys.argv[1]) )
    msg_body = open(logfile_name, 'r').read();

else:
    subject_string = "MARSS Simulation finished on host %s" % (hostname)
    msg_body = "Add something meaningful here"


consumer = OAuthEntity("anonymous", "anonymous")
user=xoauth_fields[0].strip()
destination_email=xoauth_fields[1].strip()
oauth_token = xoauth_fields[2].strip()
oauth_token_secret= xoauth_fields[3].strip()
proto="smtp"
xoauth_requestor_id=user
smtp_hostname="smtp.googlemail.com"

access_token = OAuthEntity(oauth_token, oauth_token_secret)
xoauth_string = GenerateXOauthString( consumer, access_token, user, proto, xoauth_requestor_id, None, None)

msg="From: %s\r\nTo: %s\r\nSubject: %s\r\n%s\r\n\r\n" % (user,destination_email,subject_string,msg_body)

# TODO: change this to disable the debug once it works reasonably well
# print
smtp_conn = smtplib.SMTP(smtp_hostname, 587)
smtp_conn.set_debuglevel(True)
smtp_conn.ehlo('test')
smtp_conn.starttls()
smtp_conn.ehlo('test')
smtp_conn.docmd('AUTH', 'XOAUTH ' + base64.b64encode(xoauth_string))

smtp_conn.sendmail(user, destination_email, msg)

smtp_conn.quit()
