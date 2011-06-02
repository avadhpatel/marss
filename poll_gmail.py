#!/usr/bin/python

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
imap_conn.select('marss', readonly=False)
typ, data = imap_conn.search(None, 'UNSEEN')
unreads = data[0].split()

if (len(unreads) > 0):
	# mark em all read
	ids = ",".join(unreads)
	if ids:
		typ, data = imap_conn.fetch(ids, '(RFC822)')

	status_string = get_status_string()
		
	user,dest_email,smtp_xoauth_string = generate_xoauth_string(config.xoauth_file);
	send_email(user, dest_email, smtp_xoauth_string, None, [], "STATUS", status_string)
