import os.path, time
from fabric.context_managers import lcd, settings, show
from fabric.operations import local
import fabric.state
from fabric.api import task

SHELL="/bin/bash"

def lout(cmd):
	return local(cmd, True, SHELL).stdout
	
def lsudo(cmd):
	local("sudo " + cmd)
	
def lsudoout(cmd):
	return local("sudo " + cmd, True, SHELL).stdout

def get_from_github(user, repo, update = False, after = lambda:None):
	print "installing", repo
	pwd = lout("pwd")
	if not os.path.isdir(pwd+"/"+repo):
		local("git clone https://github.com/{0}/{1}".format(user, repo))
	elif update:
		with lcd(repo):
			local("git pull")
	with lcd(repo):
		after()

def install_from_github(user, repo, update = False, before = lambda:None, after = lambda:None):	
	def after2():
		before()
		local("mvn install -DskipTests=true")
		after()
	get_from_github(user, repo, update, after2)
	
def install_bundle_from_github(user, repo, update = False, before = lambda:None, after = lambda:None):
	def after2():
		after()
		local("cp target/*.jar ../{0}/deploy/".format(APACHE_SERVICEMIX))
	install_from_github(user, repo, update, before, after2)
	
def fetch(url, target = None):
	supportedTypes = {
		"deb" : "sudo dpkg -i", 
		"zip" : "unzip",
		"tar.gz" : "tar zxvf",
		"rpm" : "sudo rpm -Uvh"
	}
	nameext = url[url.rfind("/")+1:]
	for ft in supportedTypes: 
		i = nameext.find(ft)
		if i != -1:
			name = nameext[:i-1]
			filetype = ft
			extractor = supportedTypes[ft]
			break
	
	pwd = lout("pwd")
	print "installing", name
	if target == None:
		target = pwd+"/"+name
	if os.path.isdir(target):
		print name, "already installed"
		return False
	else:
		if os.path.isfile(pwd+"/"+name+"."+filetype):
			print "file ", name+"."+filetype, "already exists, skip downloading"
		else:
			local("wget "+url)
		local(extractor + " " + name + "." + filetype)
		return True
	
# mode
mode=""
# irods zone
zoneName="tempZone"
# irods user
user="rods"
# irods password
password="rods"
# irods host
irodshost=lout("hostname")
# amqp host
hostname=lout("hostname")
# database username
testUsername="irods"
# database password
testPassword="irods"
# irods version
IRODS_VER="4.0.3"
# db version
DB_VERSION="1.3"

IRODS_HOME="/var/lib/irods"
IRODS_CONFIG="/etc/irods"
IRODS_RULES=IRODS_CONFIG
IRODS_CMD=IRODS_HOME+"/iRODS/server/bin/cmd"
ICOMMANDS="/usr/bin"
SERVICEMIX_VERSION="5.0.1"
APACHE_SERVICEMIX="apache-servicemix-"+SERVICEMIX_VERSION

OS=lout("lsb_release -si")
ARCH=lout("uname -m | sed 's/x86_//;s/i[3-6]86/32/'")
VER=lout("lsb_release -sr")

if OS == "CentOS":
	centos=True
else:
	centos=False

if mode == "irods":
	install_irods=True
	install_indexing=False
elif mode == "indexing":
	install_irods=False
	install_indexing=True
else:
	install_irods=True
	install_indexing=True
	
@task
def install(running = False, stdout = False, stderr = False):
	fabric.state.output["stdout"] = stdout
	fabric.state.output["running"] = running
	fabric.state.output["stderr"] = stderr
	with settings(warn_only=True):
		info_start()
		stop_all_services()
		mk_dirs()
		install_pkgs()
		if install_irods:
			irods()
		if install_indexing:
			indexing()
		info_finish()
	
@task
def stop_all_services():
	print "stop all services"
	with settings(warn_only=True):
		with lcd("indexing"):
			lsudo("service irods stop || true")
			lsudo("service postgresql stop || true")
			local(APACHE_SERVICEMIX+"/bin/stop || true")
			local("curl -XPOST 'http://localhost:9200/_shutdown' || true")
 
@task
def start_all_services():
	print "start all services"
	with settings(warn_only=True):
		with lcd("indexing"):
			lsudo("service postgresql start")
			lsudo("service irods start")
			local("nohup elasticsearch-1.1.1/bin/elasticsearch &")
			local(APACHE_SERVICEMIX+"/bin/start")
			
@task
def mk_dirs():
	if not os.path.isdir("indexing"):
		local("mkdir indexing")
		
@task
def install_pkgs():
	package_map = {
		"curl" : "curl",
		"openjdk-7-jdk" : "java-1.7.0-openjdk java-1.7.0-openjdk-devel",
		"maven" : "",
		"" : "byacc",
		"git" : "git",
		"xmlstarlet" : "xmlstarlet",
		"gcc" : "gcc",
		"g++" : "gcc-c++",
		"cmake" : "cmake",
		"uuid-dev" : "libuuid-devel",
		"swig" : "swig",
		"python-dev" : "python-devel",
		"postgresql" : "postgresql93-server postgresql93-client"
	}
	print 'installing required packages'
	if centos: 
		if fetch("http://dl.fedoraproject.org/pub/epel/6/x86_64/epel-release-6-8.noarch.rpm") and fetch("http://yum.postgresql.org/9.3/redhat/rhel-6-x86_64/pgdg-centos93-9.3-1.noarch.rpm"):
			lsudo("yum install " + " ".join(package_map.values()))
	else:
		lsudo("apt-get install -y " + " ".join(package_map.keys()))
		
@task
def irods():
	install_irods_icat()
	install_qpid()
	install_databook_irods_addons()
	
@task		
def indexing():
	install_servicemix()
	install_elasticsearch()
	print "installing bundles"
	install_erilex()
	install_indexing_framework()
	install_esindexer()
	install_camel_router()
	
@task	
def uninstall_irods_icat(drop_db = False):
	if not os.path.isdir("/var/lib/irods"):
		print "irods not installed"
	else:
		lsudo("apt-get purge -y irods-icat irods-database-plugin-postgres")
		lsudo("userdel irods")
		# lsudo("rm /var/lib/irods /etc/irods")
	
	if drop_db:
		lsudo("apt-get purge -y postgresql-9.3")
		
@task	
def install_irods_icat():
	if fetch("ftp://ftp.renci.org/pub/irods/releases/{0}/irods-icat-{0}-64bit.deb".format(IRODS_VER), "/var/lib/irods") and fetch("ftp://ftp.renci.org/pub/irods/releases/{0}/irods-database-plugin-postgres-{1}.deb".format(IRODS_VER, DB_VERSION)):
		lsudo("apt-get -f install") 
		lsudo("service postgresql start") 
		lsudo("su - postgres -c \"psql -c \\\"CREATE USER {0} WITH PASSWORD '{1}';\\\"; psql -c 'CREATE DATABASE \\\"ICAT\\\"'; psql -c 'GRANT ALL PRIVILEGES ON DATABASE \\\"ICAT\\\" TO {0}'\"".format(testUsername, testPassword))
		print "======================================="
		print "make sure you set zone name to {0}".format(zoneName)
		print "======================================="
		with lcd("/var/lib/irods"):
			with show("stdout"):
				lsudo("packaging/setup_irods.sh") 


@task			
def install_qpid():	
	with lcd("indexing"):
		if fetch("http://mirror.symnds.com/software/Apache/qpid/proton/0.7/qpid-proton-0.7.tar.gz"):
			with lcd("qpid-proton-0.7"):
				local("mkdir build")
				with lcd("build"):
					local("cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DSYSINSTALL_BINDINGS=ON")
					local("make all")
					lsudo("make install")
@task						
def install_databook_irods_addons():
	with lcd("indexing"):
		def after():
			if not os.path.isfile(IRODS_CMD + "/file"):
				lsudo("ln -s `which file` {0}/file".format(IRODS_CMD))
				lsudo("chown irods:irods {0}/file".format(IRODS_CMD)) 
			lsudo("cp amqpsend.py {0}".format(IRODS_CMD))
			lsudo("chmod +x {0}/amqpsend.py".format(IRODS_CMD))
			lsudo("chown irods:irods {0}/amqpsend.py".format(IRODS_CMD)) 
			lsudo("cp *.re {0}".format(IRODS_RULES)) 
			lsudo("chown irods:irods {0}/*.re".format(IRODS_RULES)) 
			lsudo("sed -i 's/^\\([^\\\"]*\\\"\\)localhost\\(.*\\)/\\1{0}\\2/' {1}/databook.re".format(hostname, IRODS_RULES))
			a=lsudoout("grep amqp {0}/server.config || true".format(IRODS_CONFIG))
			if a != "":
				print "server.config is already edited"
			else:
				lsudo("sed -i 's/^\\(reRuleSet *\\)\\([^ ]*\\)/\\1amqp,databook_pep,databook,\\2/' {0}/server.config".format(IRODS_CONFIG))
		get_from_github("DICE-UNC", "indexing-irods", after = after)

@task
def install_servicemix():
	with lcd("indexing"):
		name = APACHE_SERVICEMIX
		if fetch("http://archive.apache.org/dist/servicemix/servicemix-5/{0}/apache-servicemix-{0}.zip".format(SERVICEMIX_VERSION)):
			with lcd(name):
				local("bin/start")
				print("waiting for servicemix")
				time.sleep(10)
				local("bin/client -p 8101 -h localhost -u karaf -u karaf features:install camel-jms")
				local("bin/stop")
				local('''xmlstarlet ed -N ns="http://activemq.apache.org/schema/core" \\
					--append "//ns:transportConnector[@name='openwire']" \\
					--type elem \\
					-n transportConnector \\
					--value "" \\
					--subnode "//transportConnector[not(@name)]" \\
					--type attr \\
					-n name \\
					--value "amqp" \\
					--subnode "//transportConnector[@name='amqp']" \\
					--type attr \\
					-n uri \\
					--value "amqp://0.0.0.0:5672" \\
					--append "//transportConnector[@name='amqp']" \\
					--type elem \\
					-n transportConnector \\
					--value "" \\
					--subnode "//transportConnector[not(@name)]" \\
					--type attr \\
					-n name \\
					--value "websocket" \\
					--subnode "//transportConnector[@name='websocket']" \\
					--type attr \\
					-n uri \\
					--value "ws://0.0.0.0:61614" \\
					--delete "//ns:jaasAuthenticationPlugin" \\
					etc/activemq.xml > etc/activemq.xml.new''')
				local("mv etc/activemq.xml etc/activemq.xml.bak")
				local("mv etc/activemq.xml.new etc/activemq.xml")
				
@task		   
def install_elasticsearch():
	with lcd("indexing"):
		if fetch("https://download.elasticsearch.org/elasticsearch/elasticsearch/elasticsearch-1.1.1.tar.gz"):
			if centos:
				print "disable multicast on CentOS"
				local("sed -i 's/^[# ]*discovery.zen.ping.multicast.enabled:.*/discovery.zen.ping.multicast.enabled: false/' elasticsearch-1.1.1/config/elasticsearch.yml")

			local("sed -i 's/^[# ]*cluster.name:.*/cluster.name: databookIndexer/' elasticsearch-1.1.1/config/elasticsearch.yml")
			local("nohup elasticsearch-1.1.1/bin/elasticsearch &")
			print "waiting for elasticsearch"
			time.sleep(10)
			local("curl -XPUT 'http://localhost:9200/databook'")
			local("curl -XPUT 'http://localhost:9200/databook/entity/_mapping' -d '{\"properties\":{\"uri\":{\"type\":\"string\", \"index\":\"not_analyzed\"}, \"type\":{\"type\":\"string\", \"index\":\"not_analyzed\"}}}'") 
			
@task
def install_erilex():
	with lcd("indexing"):
		install_from_github("xu-hao","erilex")
		
@task
def install_indexing_framework():
	with lcd("indexing"):
		def before():
			local('''
				sed -i 	-e "s/^\(irods[.]host=\).*/\\1{0}/" \\
				-e "s/^\(irods[.]home=\/\)[^/]*\(\/.*.\)/\\1{1}\\2/" \\
				-e "s/^\(irods[.]zone=\).*/\\1{1}/" \\
				-e "s/^\(irods[.]user=\).*/\\1{2}/" \\
				-e "s/^\(irods[.]password=\).*/\\1{3}/" \\
				src/databook/config/irods.properties'''.format(irodshost, zoneName, user, password))
		install_bundle_from_github("DICE-UNC","indexing",before = before)
		
@task
def install_camel_router():
	with lcd("indexing"):
		install_bundle_from_github("DICE-UNC","indexing-camel-router")
		
@task
def install_esindexer():
	with lcd("indexing"):
		install_bundle_from_github("DICE-UNC", "elasticsearch")

def info_start():
	print '''=============================================
this is a demo installatin script
don't run this script on a production system!
this script is tested on Ubuntu 14.04 LTS / CentOS 6.5 (install iRODS manually)
============================================='''
	print "mode:", mode
	print "iRODS host:", irodshost
	print "iRODS zone:", zoneName
	print "iRODS user:", user
	print "iRODS password:", password
	print "AMQP host:", hostname
	print "OS:", OS
	print "version:", VER
	print "architecture:", ARCH

def info_finish():
	print '''
==========================================
done
start the services in the following order:
irods | elasticsearch servicemix
==========================================
to start elasticsearch run: 
indexing/elasticsearch-1.1.1/bin/elasticsearch
to start irods run: 
sudo service postgresql start
sudo service irods start
to start servicemix run:
indexing/{0}/bin/servicemix server
to start search gui run:
firefox indexing/elasticsearch/src/index.html
to test run the follow commands:
wget http://www.gutenberg.org/cache/epub/19033/pg19033.txt
wget http://www.gutenberg.org/cache/epub/1661/pg1661.txt
iput pg19033.txt
iput pg1661.txt
'''.format(APACHE_SERVICEMIX)

