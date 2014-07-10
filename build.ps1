# an example build command in windows 
if (-not (test-path build -pathtype container )) {
	mkdir build
}
cd build
cmake .. `
	-DBOOST_ROOT=C:\boost_1_47_0 `
	"-DBOOST_LIBRARYDIR=C:\boost_1_47_0\stage" `
	-DCMAKE_INCLUDE_PATH=C:\glew-1.10.0\include `
	-DCMAKE_FRAMEWORK_PATH=C:\glew-1.10.0\lib\Release\Win32 `
	-DPKG_CONFIG_EXECUTABLE=C:\pkg-config\bin\pkg-config.exe `
	-DSWIG_DIR=C:\swigwin-3.0.2\ `
	-DSWIG_EXECUTABLE=C:\swigwin-3.0.2\swig.exe `
	"-DFREETYPE2_DIR=C:\freetype" `
    	-DCPPUNIT_DIR=C:/cppunit `
	-DGLFW3_INCLUDE_DIR=C:\glfw-3.0.4\include `
	-DGLFW3_LIBRARY_DIR=C:\glfw-3.0.4\build\src\Release `
	"-DGNURADIO_LIBRARY_DIR=c:/Program Files (x86)/gnuradio/lib" `
	"-DGNURADIO_INCLUDE_DIR=c:/Program Files (x86)/gnuradio/include" `

cd ..
