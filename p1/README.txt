`$> make` compiles the source code
The following commands use variables that can be set via command line arguments, see the top of the makefile for the variable names and their default values. Note the root commands `#>`
	- `#> make install` copies the necessary files to the cgi directory and sets their permissions
	- `$> make configure_dryrun` outputs the changes to the config file with variable values, but without modifying the file
	- `#> make configure` applies changes to the config file to enable running cgi scripts directly via browser url
	- `#> make server_restart` restarts server service to apply configuration changes
