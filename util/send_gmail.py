#!/usr/bin/python -W ignore::DeprecationWarning

# this script will generate send an email to notify you when a simulation
# finishes. It is meant to be used by adding 
#  -execute-after-kill "python util/send_gmail.py" 
# to your simconfig file. 
#
# To use this script, please follow the instructions below (using the sample
# xoauth.py script from google's examples to generate the credentials) and
# recording the data into the xoauth.txt file


import smtplib
from email.mime.image import MIMEImage
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

import base64
from xoauth import * 
import random
import os
import socket

import config

def instructions(xoauth_txt_path):
	print """
		Could not find %s...
		Please generate %s ; first run: 
			./xoauth.py --generate_oauth_token --user=YOUR_USERNAME@gmail.com

		For more details, please see the README file. 
		
	""" % (xoauth_txt_path, xoauth_txt_path)

def generate_xoauth_string(xoauth_txt_path,proto_="smtp"):
	xoauth_cred = open(xoauth_txt_path)

	# readlines returns an array of lines 
	xoauth_fields = file.readlines(xoauth_cred); 

	#fill in the variables needed for an xoauth entity/string from the file
	user=xoauth_fields[0].strip()
	oauth_token = xoauth_fields[1].strip()
	oauth_token_secret= xoauth_fields[2].strip()
	proto=proto_
	xoauth_requestor_id=user
	consumer = OAuthEntity("anonymous", "anonymous")

	access_token = OAuthEntity(oauth_token, oauth_token_secret)
	xoauth_string = GenerateXOauthString( consumer, access_token, user, proto, xoauth_requestor_id, None, None)
	return (user,xoauth_string)

def send_simulation_finished(user, destination_email, xoauth_string, simulation_num=None, imagefile_names=None, msg_body=""):
	hostname = socket.gethostname()
	if simulation_num != None:
		subject_string = "MARSS Simulation %d finished on host %s" % (simulation_num, hostname)
	else:
		subject_string = "MARSS Simulation finished on host %s" % (hostname)
	send_email(user, destination_email, xoauth_string, simulation_num, imagefile_names, subject_string, msg_body)

def send_email(user, destination_email, xoauth_string, simulation_num=None, imagefile_names=None, subject_string="simulation", msg_body=""):
	smtp_hostname="smtp.googlemail.com"

	msg = MIMEMultipart()
	msg['Subject'] = subject_string
	msg['From'] = user
	msg['To'] = destination_email
	msg_text = MIMEText(msg_body)
	msg.attach(msg_text)

#	msg.preamble = msg_body
	for imagefile_name in imagefile_names:
		if imagefile_name != None:
			fp = open(imagefile_name, 'rb');
			img = MIMEImage(fp.read())
			fp.close()
			msg.attach(img); 

	smtp_conn = smtplib.SMTP(smtp_hostname, 587)
	#smtp_conn.set_debuglevel(True)
	smtp_conn.ehlo('test')
	smtp_conn.starttls()
	smtp_conn.ehlo('test')
	smtp_conn.docmd('AUTH', 'XOAUTH ' + base64.b64encode(xoauth_string))

	smtp_conn.sendmail(user, destination_email, msg.as_string())

	smtp_conn.quit()
def authorize_and_send(simulation_num=None,image_files=[],strings_arr=[]):

	if not os.path.exists(config.get_xoauth_filename()):
		instructions(config.get_xoauth_filename());
		exit();
	msg=""
	for s in strings_arr:
		msg = msg + "\n" + s;
#		print s
		
	user,xoauth_string = generate_xoauth_string(config.get_xoauth_filename())
	send_simulation_finished(user,config.get_destination_email() ,xoauth_string,simulation_num,image_files,msg_body=msg); 
	print "Simulation finished message sent";

def read_options():
	"""Read command line options"""
	parser = OptionParser()
	parser.add_option("-m", "--message", dest="message", type="string",
			default="", help="Email body message")

	(options, args) = parser.parse_args()
	return (options, args)


if __name__ == "__main__":
	(options, args) = read_options()
	if options.message == "":
		options.message = "This is a simple test -- if you're getting this message, the xoauth is working properly"

	authorize_and_send(None, [], [options.message])
