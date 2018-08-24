

function install_gtest()
{
	mkdir -p $BUILD_DIR/googletest
	cd $BUILD_DIR/googletest
	cmake ~/src/googletest
	make
	sudo make install
}

function install_ktf()
{
	cd $INSTALL_DIR/ktf
	autoreconf

	mkdir -p $BUILD_DIR/ktf
	cd $BUILD_DIR/ktf
	~/src/ktf/configure KVER=`uname -r`
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
    sudo git clone https://github.com/google/googletest.git
    sudo git clone https://github.com/oracle/ktf.git
    
    install_package
    install_gtest
    install_ktf
}