#!/usr/bin/python -W ignore::DeprecationWarning

#this code is extremely loosely based off of: 
# http://code.google.com/p/yjl/source/browse/Python/snippet/gmail-xoauth.py
# it should be run as a cron job -- it will poll the gmail account that you
# access via xoauth and if it sees a message in the 'marss' label, it will send
# back some kind of status report email 

import imaplib
import config

from send_gmail import *
from sim_status import *

user,xoauth_imap_string = generate_xoauth_string(config.get_xoauth_filename(), "IMAP");

# TODO: perhaps this should generate a reply to the sender of the message instead 
dest_email = config.get_destination_email()
label_to_watch = 'marss'

imap_hostname = 'imap.googlemail.com'

# Get unread/unseen list
imap_conn = imaplib.IMAP4_SSL(imap_hostname)
# imap_conn.debug = 4
imap_conn.authenticate('XOAUTH', lambda x: xoauth_imap_string)
# look for the label called 'marss'; mark message as read (readonly=false)
imap_conn.select(label_to_watch, readonly=False)
typ, data = imap_conn.search(None, 'UNSEEN')
unreads = data[0].split()

if (len(unreads) > 0):
	# mark em all read
	ids = ",".join(unreads)
	if ids:
		typ, data = imap_conn.fetch(ids, '(RFC822)')

	# this string comes from sim_status.py -- you will probably need to adjust this string to taste
	status_string = get_status_string()

	# Another token is needed for the SMTP request
	user,xoauth_smtp_string = generate_xoauth_string(config.get_xoauth_filename());
	# send back an email 
	send_email(user, dest_email, xoauth_smtp_string, None, [], "Simulation status report", status_string)
else:
	print "No new messages in label '%s'"%(label_to_watch)
