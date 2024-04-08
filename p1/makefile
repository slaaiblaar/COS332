# Change these variables via CLI arguments (`make some_command variable=value`):
CGI_DIR ?= /var/www/cgi-bin/
CONFIG_FILE ?= /etc/apache2/apache2.conf
CGI_MODULE_NAME ?= cgid_module
CGI_MODULE_FILE ?= /usr/lib/apache2/modules/mod_cgid.so
SERVICE_NAME ?= apache2

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11

SRCS = fib.cxx

OBJS = $(SRCS:.cxx=.o)

EXECS = $(SRCS:.cxx=)

BASH_SCRIPTS = $(wildcard *.sh)

all: $(EXECS)

$(EXECS): %: %.o
	$(CXX) $(CXXFLAGS) $^ -o $@
	chmod +x $@

%.o: %.cxx
	$(CXX) $(CXXFLAGS) -c $< -o $@
# copies files to cgi directory and sets necessary permissions
install: $(EXECS)
	mkdir -p $(CGI_DIR)
	cp $(EXECS) $(addprefix $(CGI_DIR), $(EXECS))
	chmod +x $(addprefix $(CGI_DIR), $(EXECS) $(BASH_SCRIPTS))
	touch $(addprefix $(CGI_DIR), "fibnums.txt")
	echo "0 0 0" > $(addprefix $(CGI_DIR), "fibnums.txt")
	chmod 777 $(addprefix $(CGI_DIR), "fibnums.txt")
# prints configuration that will be appended to config file
configure_dryrun:
	echo "'''echo \nLoadModule $(CGI_MODULE_NAME) $(CGI_MODULE_FILE)\nScriptAlias /cgi-bin/ $(CGI_DIR) \n<Directory $(CGI_DIR)> \n\tRequire all granted \n\tOptions +ExecCGI \n\tDefaultType text/plain \n\tSetHandler cgi-script \n</Directory> >> $(CONFIG_FILE)'''"
# appends confiurations to config file that enables it to run cgi-scripts directly via "localhost/cgi-bin/scriptname" url
configure:
	echo "\nLoadModule $(CGI_MODULE_NAME) $(CGI_MODULE_FILE)\nScriptAlias /cgi-bin/ $(CGI_DIR) \n<Directory $(CGI_DIR)> \n\tRequire all granted \n\tOptions +ExecCGI \n\tDefaultType text/plain \n\tSetHandler cgi-script \n</Directory>" >> $(CONFIG_FILE)

server_restart:
	@echo "If restart fails, ensure that:\nthe service name, $(SERVICE_NAME), is correct\nthe cgi module name, '$(CGI_MODULE_NAME)', is correct\nthe file, '$(CGI_MODULE_FILE)', exists"
	systemctl restart $(SERVICE_NAME)

clean:
	rm -f $(OBJS) $(EXECS)
