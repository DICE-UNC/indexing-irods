echo make new directory
mkdir indexing
cd indexing

echo installing required packages
sudo apt-get install openjdk-6-jdk git xmlstarlet gcc cmake uuid-dev swig python-dev
echo make sure that openjdk-6-jdk is the default package

echo installing irods
if [ -d irods-legacy ]; then
	echo irods already installed
else
	echo make sure you set zone name to databook
	git clone https://github.com/irods/irods-legacy
	cd irods-legacy/iRODS
	./irodssetup
	cd ../..
fi
IRODS_HOME=`pwd`/irods-legacy/iRODS

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
	
	echo please run the following command: 
	echo features:install camel-jms
	echo the press CTRL-D
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


echo installing elasticsearch
sudo apt-get install elasticsearch
sudo sed -i 's/^[# ]*cluster.name:.*/cluster.name: databookIndexer/' /etc/elasticsearch/elasticsearch.yml
sudo service elasticsearch start
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
ln -s `which file` $IRODS_HOME/server/bin/cmd/file
cp amqpsend.py $IRODS_HOME/server/bin/cmd/
chmod +x $IRODS_HOME/server/bin/cmd/amqpsend.py
cp *.re $IRODS_HOME/server/config/reConfigs/
sed -i 's/^\(reRuleSet *\)\([^ ]*\)/\1amqp,databook_pep,databook,\2/' $IRODS_HOME/server/config/server.config
cd ..

echo done
cd ..
