OS=$(lsb_release -si)
ARCH=$(uname -m | sed 's/x86_//;s/i[3-6]86/32/')
VER=$(lsb_release -sr)
if [ $OS == CentOS ]; then
	centos=1
fi

if [ $1 ]; then
	zoneName=$1
else
	echo no zoneName exit
	exit
fi

SERVICEMIX_VERSION=5.0.1

echo =============================================
echo this is a demo installatin script
echo "don't run this script on a production system!"
echo "this script is tested on Ubuntu 14.04 LTS / CentOS 6.5 (install iRODS manually)"
echo press ENTER to continue
echo =============================================
read enter
sudo service irods stop
sudo service postgresql stop

echo kill all processes
sudo killall java postgres irodsServer irodsReServer 

echo make new directory
mkdir indexing
cd indexing

if [ "$centos" ]; then
	if [ -e epel-release-6-8.noarch.rpm ]; then
		echo epel-release-6-8.noarch.rpm already exists, skip downloading
	else 
		wget http://dl.fedoraproject.org/pub/epel/6/x86_64/epel-release-6-8.noarch.rpm
	fi
	sudo rpm -Uvh epel-release-6-8.noarch.rpm
	sudo yum install curl java-1.7.0-openjdk java-1.7.0-openjdk-devel byacc git xmlstarlet gcc gcc-c++ cmake libuuid-devel swig python-devel
else
	echo installing required packages
	sudo apt-get install maven curl openjdk-7-jdk git xmlstarlet gcc cmake uuid-dev swig python-dev 
	sudo apt-get install postgresql g++
fi

echo installing irods
if [ -e /var/lib/irods ]; then
	echo irods already installed
else
    wget ftp://ftp.renci.org/pub/irods/releases/4.0.2/irods-icat-4.0.2-64bit.deb
    wget ftp://ftp.renci.org/pub/irods/releases/4.0.2/irods-database-plugin-postgres-1.2.deb
    sudo dpkg -i irods-icat-4.0.2-64bit.deb irods-database-plugin-postgres-1.2.deb
	sudo apt-get -f install
	sudo service postgresql start
	echo enter database username:
	read testUsername
	echo enter database password:
	read testPassword
	sudo su - postgres -c "psql -c \"CREATE USER $testUsername WITH PASSWORD '$testPassword';\"; psql -c 'CREATE DATABASE \"ICAT\"'; psql -c 'GRANT ALL PRIVILEGES ON DATABASE \"ICAT\" TO $testUsername'"
	echo =======================================
	echo make sure you set zone name to $zoneName
	echo =======================================
	sudo su - irods -c "packaging/setup_database.sh"
fi
IRODS_HOME=/var/lib/irods
IRODS_CONFIG=/etc/irods
IRODS_RULES=$IROD_CONFIG
IRODS_CMD=$IRODS_HOME/iRODS/server/bin/cmd
ICOMMANDS=/usr/bin

echo installing servicemix
if [ -d apache-servicemix-$SERVICEMIX_VERSION ]; then
	echo service mix already installed
else
	if [ -e apache-servicemix-$SERVICEMIX_VERSION.zip ]; then
		echo file apache-servicemix-$SERVICEMIX_VERSION.zip already exists, skip downloading
	else
		wget http://archive.apache.org/dist/servicemix/servicemix-5/$SERVICEMIX_VERSION/apache-servicemix-$SERVICEMIX_VERSION.zip
	fi
	unzip apache-servicemix-$SERVICEMIX_VERSION.zip
	cd apache-servicemix-$SERVICEMIX_VERSION
	
	bin/start
	echo waiting for servicemix
	sleep 10
	bin/client -p 8101 -h localhost -u karaf -u karaf features:install camel-jms
	bin/stop
	xmlstarlet ed -N ns="http://activemq.apache.org/schema/core" \
		   --append "//ns:transportConnector[@name='openwire']" \
		   --type elem \
		   -n transportConnector \
		   --value "" \
		   --subnode "//transportConnector[not(@name)]" \
		   --type attr \
		   -n name \
		   --value "amqp" \
		   --subnode "//transportConnector[@name='amqp']" \
		   --type attr \
		   -n uri \
		   --value "amqp://0.0.0.0:5672" \
		   --append "//transportConnector[@name='amqp']" \
		   --type elem \
		   -n transportConnector \
		   --value "" \
		   --subnode "//transportConnector[not(@name)]" \
		   --type attr \
		   -n name \
		   --value "websocket" \
		   --subnode "//transportConnector[@name='websocket']" \
		   --type attr \
		   -n uri \
		   --value "ws://0.0.0.0:61614" \
		   --delete "//ns:jaasAuthenticationPlugin" \
		   etc/activemq.xml > etc/activemq.xml.new
		   
	mv etc/activemq.xml etc/activemq.xml.bak
	mv etc/activemq.xml.new etc/activemq.xml
		   
		   
	cd ..
fi
APACHE_SERVICEMIX=`pwd`/apache-servicemix-$SERVICEMIX_VERSION

echo installing qpid messenger
if [ -d qpid-proton-0.7 ]; then
	echo qpid messenger already installed
else
	if [ -e qpid-proton-0.7.tar.gz ]; then
		echo file qpid-proton-0.7.tar.gz already exists, skip downloading
	else
		wget http://mirror.symnds.com/software/Apache/qpid/proton/0.7/qpid-proton-0.7.tar.gz
	fi
	tar zxvf qpid-proton-0.7.tar.gz
	cd qpid-proton-0.7
	mkdir build
	cd build
	cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DSYSINSTALL_BINDINGS=ON
	make all
	sudo make install
	cd ../..
fi

echo installing elasticsearch
if [ -e elasticsearch-1.1.1.tar.gz ]; then
	echo file elasticsearch-1.1.1.tar.gz already exists, skip downloading
else
	wget https://download.elasticsearch.org/elasticsearch/elasticsearch/elasticsearch-1.1.1.tar.gz
fi
tar zxvf elasticsearch-1.1.1.tar.gz
if [ "$centos" ]; then
	echo disable multicast on CentOS
	sed -i 's/^[# ]*discovery.zen.ping.multicast.enabled:.*/discovery.zen.ping.multicast.enabled: false/' elasticsearch-1.1.1/config/elasticsearch.yml
fi
sed -i 's/^[# ]*cluster.name:.*/cluster.name: databookIndexer/' elasticsearch-1.1.1/config/elasticsearch.yml
nohup elasticsearch-1.1.1/bin/elasticsearch &
echo waiting for elasticsearch
sleep 10
curl -XPUT 'http://localhost:9200/databook'
curl -XPUT 'http://localhost:9200/databook/entity/_mapping' -d '{"properties":{"uri":{"type":"string", "index":"not_analyzed"}, "type":{"type":"string", "index":"not_analyzed"}}}' 

echo installing bundles

echo 	erilex
git clone https://github.com/xu-hao/erilex
cd erilex
mvn install
cd ..

echo 	indexing
git clone https://github.com/DICE-UNC/indexing
cd indexing
sed -i "s/^\(irods[.]home=\/\)[^/]*\(\/.*.\)/\1$zoneName\2/" src/databook/config/irods.properties
sed -i "s/^\(irods[.]zone=\).*/\1$zoneName/" src/databook/config/irods.properties 
mvn install -DskipTests=true
cp target/*.jar $APACHE_SERVICEMIX/deploy/
cd ..

echo 	camel-router 
git clone https://github.com/DICE-UNC/indexing-camel-router 
cd indexing-camel-router 
mvn install 
cp target/*.jar $APACHE_SERVICEMIX/deploy/ 
cd ..

echo 	elasticsearch-indexer
git clone https://github.com/DICE-UNC/elasticsearch
cd elasticsearch
mvn install
cp target/*.jar $APACHE_SERVICEMIX/deploy/
cd ..

echo installing config files
git clone https://github.com/DICE-UNC/indexing-irods
cd indexing-irods
sudo ln -s `which file` $IRODS_CMD/file
sudo cp amqpsend.py $IRODS_CMD
sudo chmod +x $IRODS_CMD/amqpsend.py
sudo cp *.re $IRODS_RULES
sudo chown irods:irods $IRODS_RULES/*.re

a=`grep amqp $IRODS_CONFIG/server.config`
if [ "$a" != "" ]; then
	echo server.config is already edited
else
	sudo sed -i 's/^\(reRuleSet *\)\([^ ]*\)/\1amqp,databook_pep,databook,\2/' $IRODS_CONFIG/server.config
fi
cd ..

echo ==========================================
echo done
echo start the services in the following order:
echo "irods | elasticsearch servicemix"
echo ==========================================
echo to start elasticsearch run: 
echo indexing/elasticsearch-1.1.1/bin/elasticsearch
echo to start irods run: 
echo sudo service postgresql start
echo sudo service irods start
echo to start servicemix run:
echo $APACHE_SERVICEMIX/bin/servicemix server
echo to start search gui run:
echo firefox indexing/elasticsearch/src/index.html
echo to test run the follow commands:
echo wget http://www.gutenberg.org/cache/epub/19033/pg19033.txt
echo wget http://www.gutenberg.org/cache/epub/1661/pg1661.txt
echo iput pg19033.txt
echo iput pg1661.txt

cd ..
