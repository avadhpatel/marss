import os
import ConfigParser

config = None


def read_config(conf_file = "util.cfg"):
	global config

	if not os.path.exists(conf_file):
		print("Unable to read '%s' configuration file." % conf_file)
		exit(-1)

	config = ConfigParser.SafeConfigParser()
	config.read(conf_file)

def check_config_param(config, section, param, is_path=False):
	if config.has_option(section, param):
		if is_path and not os.path.exists(config.get(section, param)):
			print("'%s' parameter in your config file is not valid." % param)
			print("Please fix this error.")
			exit(-1)

		return True

	print("Please specify '%s' in your configuration file." % param)
	exit(-1)

def get_xoauth_filename():
	if not config:
		read_config()
	check_config_param(config, 'email', 'xoauth', True)
	return config.get('email', 'xoauth')

def get_marss_dir_path(filename):
	if not config:
		read_config()
	check_config_param(config, 'marss', 'marss_dir', True)
	return "%s/%s" % (config.get('marss', 'marss_dir'), filename)

def get_destination_email():
	if not config:
		read_config()
	check_config_param(config, 'email', 'to')
	return config.get('email', 'to')
