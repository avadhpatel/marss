import os
import ConfigParser

config = None


def read_config(conf_file = None):
	global config

	if not conf_file:
		# Set default conf file to be "./util.cfg"
		curr_dir = os.path.dirname(os.path.realpath(__file__))
		conf_file = "%s/util.cfg" % curr_dir
        print("Reading config file %s" % conf_file)

	if not os.path.exists(conf_file):
		print("Unable to read '%s' configuration file." % conf_file)
		exit(-1)

	config = ConfigParser.ConfigParser()
	config.read(conf_file)

	# Store the config file path
	config.add_section('util')
	config.set('util', 'dir', os.path.dirname(conf_file))

	return config

def check_config_param(config, section, param, is_path=False):
	if config.has_option(section, param):
		# If specified parameter is relative path then use config
		# file path to get full path
		if is_path and not os.path.isabs(config.get(section, param)):
			full_path = "%s/%s" % (config.get('util', 'dir'),
					config.get(section, param))
			config.set(section, param, full_path)

		if is_path and not os.path.exists(config.get(section, param)):
			print("'%s' parameter in your config file is not valid." % param)
			print("Error: Can't find file/directory: %s" % (
				config.get(section, param)))
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
	check_config_param(config, 'DEFAULT', 'marss_dir', True)
	return "%s/%s" % (config.get('DEFAULT', 'marss_dir'), filename)

def get_destination_email():
	if not config:
		read_config()
	check_config_param(config, 'email', 'to')
	dest_email = config.get('email', 'to')
	if dest_email == "youremail@domain.com":
		print "Please change the default destination email before using this script"
		exit()
	else:
		return dest_email 
