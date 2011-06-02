#!/usr/bin/python

#this code is extremely loosely based off of: 
# http://code.google.com/p/yjl/source/browse/Python/snippet/gmail-xoauth.py
# it should be run as a cron job -- it will poll the gmail account that you
# access via xoauth and if it sees a message in the 'marss' label, it will send
# back some kind of status report email 

import imaplib
import config

from send_gmail import *
from sim_status import *

user,dest_email,xoauth_string = generate_xoauth_string(config.xoauth_file, "IMAP");

imap_hostname = 'imap.googlemail.com'


# Get unread/unseen list
imap_conn = imaplib.IMAP4_SSL(imap_hostname)
# imap_conn.debug = 4
imap_conn.authenticate('XOAUTH', lambda x: xoauth_string)
# Set readonly, so the message won't be set with seen flag
# look for the label called 'marss'
imap_conn.select('marss', readonly=False)
typ, data = imap_conn.search(None, 'UNSEEN')
unreads = data[0].split()

if (len(unreads) > 0):
	# mark em all read
	ids = ",".join(unreads)
	if ids:
		typ, data = imap_conn.fetch(ids, '(RFC822)')

	# this string comes from sim_status.py
	status_string = get_status_string()
	user,dest_email,smtp_xoauth_string = generate_xoauth_string(config.xoauth_file);
	# send back an email called STATUS
	send_email(user, dest_email, smtp_xoauth_string, None, [], "STATUS", status_string)
