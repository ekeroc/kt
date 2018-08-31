

function install_gtest()
{
	mkdir -p $BUILD_DIR/googletest
	cd $BUILD_DIR/googletest
	cmake $INSTALL_DIR/googletest
	make
	sudo make install
}

function install_ktf()
{
	cd $INSTALL_DIR/ktf
	autoreconf

	mkdir -p $BUILD_DIR/ktf
	cd $BUILD_DIR/ktf
	$INSTALL_DIR/ktf/configure KVER=`uname -r`
	make
	sudo make install
}

function install_package()
{
    sudo apt-get install -y \
	git \
	cmake \
    autoconf \
	libnl-3-dev \
	libnl-genl-3-dev
}

function ktfff_setup()
{
    mkdir -p $INSTALL_DIR
	mkdir -p $BUILD_DIR

    cd $INSTALL_DIR
	
	if [ ! -d "$INSTALL_DIR/googletest" ] ; then
		sudo git clone https://github.com/google/googletest.git
	else
		cd $INSTALL_DIR/googletest
		sudo git pull
		cd $KTFFF_DIR
	fi

 	if [ ! -d "$INSTALL_DIR/ktf" ] ; then
		sudo git clone https://github.com/oracle/ktf.git
	else
		cd $INSTALL_DIR/ktf
		sudo git pull
		cd $KTFFF_DIR
	fi
    
    install_package
    install_gtest
    install_ktf

	sudo rm -rf /usr/lib/libktf.*
	sudo ln -s /usr/local/lib/libktf.so* /usr/lib
	sudo ldconfig
}

function ktfff_uninstall()
{	
	sudo rm -rf /usr/lib/libktf.*
	sudo rm -rf /usr/local/lib/libktf.*
	sudo rm -rf $KTFFF_BASE_DIR
	ktfff_info "KTFFF UNINSTALL."
}