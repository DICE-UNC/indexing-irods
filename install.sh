echo =============================================
echo this is a demo installatin script
echo "don't run this script on a production system!"
echo press ENTER to continue
echo =============================================
read enter
echo kill all processes
sudo killall java postgres irodsServer irodsReServer 

echo make new directory
mkdir indexing
cd indexing

echo installing required packages
sudo apt-get install openjdk-6-jdk git xmlstarlet gcc cmake uuid-dev swig python-dev
echo make sure that openjdk-6-jdk is the default package

echo installing servicemix
if [ -d apache-servicemix-5.0.0 ]; then
	echo service mix already installed
else
	if [ -e apache-servicemix-5.0.0.zip ]; then
		echo file apache-servicemix-5.0.0.zip already exists, skip downloading
	else
		wget http://mirror.olnevhost.net/pub/apache/servicemix/servicemix-5/5.0.0/apache-servicemix-5.0.0.zip
	fi
	unzip apache-servicemix-5.0.0.zip
	cd apache-servicemix-5.0.0
	
	echo =================================
	echo please run the following command: 
	echo features:install camel-jms
	echo then press CTRL-D
	echo =================================
	bin/servicemix
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
APACHE_SERVICEMIX=`pwd`/apache-servicemix-5.0.0

echo installing irods
if [ -d irods-legacy ]; then
	echo irods already installed
else
	echo =======================================
	echo make sure you set zone name to databook
	echo =======================================
	git clone https://github.com/irods/irods-legacy
	cd irods-legacy/iRODS
	./irodssetup
	cd ../..
fi
IRODS_HOME=`pwd`/irods-legacy/iRODS
IRODS_CONFIG=$IRODS_HOME/server/config
IRODS_RULES=$IRODS_HOME/server/config/reConfigs
IRODS_CMD=$IRODS_HOME/server/bin/cmd
ICOMMANDS=$IRODS_HOME/clients/icommands

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
sudo sed -i 's/^[# ]*cluster.name:.*/cluster.name: databookIndexer/' elasticsearch-1.1.1/config/elasticsearch.yml
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
ln -s `which file` $IRODS_CMD/file
cp amqpsend.py $IRODS_CMD
chmod +x $IRODS_CMD/amqpsend.py
cp *.re $IRODS_RULES

a=`grep amqp $IRODS_CONFIG/server.config`
if [ "$a" != "" ]; then
	echo server.config is already edited
else
	sed -i 's/^\(reRuleSet *\)\([^ ]*\)/\1amqp,databook_pep,databook,\2/' $IRODS_CONFIG/server.config
fi
cd ..

echo ========
echo all done
echo ========
echo to start elasticsearch run: 
echo indexing/elasticsearch-1.1.1/bin/elasticsearch
echo to start irods run: 
echo $IRODS_HOME/irodsctl restart
echo to start servicemix run:
echo $APACHE_SERVICEMIX/bin/servicemix server
echo to start search gui run:
echo firefox indexing/elasticsearch-1.1.1/src/index.html
echo to test run the follow commands:
echo wget http://www.gutenberg.org/cache/epub/19033/pg19033.txt
echo wget http://www.gutenberg.org/cache/epub/1661/pg1661.txt
echo $ICOMMANDS/bin/iput pg19033.txt
echo $ICOMMANDS/bin/iput pg1661.txt
cd ..
