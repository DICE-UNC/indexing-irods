indexing-irods
==============

config files for iRODS for utilizing the indexing framework

requirements:

* Install the file command, make symbolic link file to the system file command under server/bin/cmd 
* Install python
* Install Apache QPid Messenger 0.7
* Copy amqpsend.py to server/bin/cmd, grant exec permission
* Copy *.re to server/config/reConfigs (3.3.1) or /etc/irods (4.0.0)
* Edit server/config/server.config (3.3.1) or /ect/irods/server.config, add amqp,databook,databook_pep before core
