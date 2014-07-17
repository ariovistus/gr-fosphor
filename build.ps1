param (
[switch] $choco = $false
);

# an example build command in windows 

$gnuradio = "c:/Program Files (x86)/gnuradio"
$glew = "C:\glew-1.10.0"
$glew_path = "$glew\lib\Release\Win32"
$glew_include = "$glew\include" 
$pkgconfig_exe = "C:\pkg-config\bin\pkg-config.exe" 
$boost = "C:/boost_1_54_0"
$cppunit = "C:/cppunit"
$freetype = "C:\freetype"
$swig = "C:\swigwin-3.0.2"
$glfw = "C:\glfw-3.0.4"

Function mkdirp {
    param (
        [String] $path
    )
    if (-not (test-path $path -pathtype container)) {
        mkdir $path
    }
}


if ($choco) {
    $todir = (get-item "chocolatey").FullName
    echo "$todir"
    mkdirp "$todir\dist\bin"
    mkdirp "$todir\dist\py-fosphor"
    mkdirp "$todir\dist\blocks"
    cp build\lib\Release\gnuradio-fosphor.dll "$todir\dist\bin"
    cp "$glew\bin\Release\Win32\glew32.dll" "$todir\dist\bin"
    cp "$freetype\bin\freetype6.dll" "$todir\dist\bin"
    cp build\swig\Release\_fosphor_swig.pyd "$todir\dist\py-fosphor"
    cp build\swig\fosphor_swig.py "$todir\dist\py-fosphor"
    cp python\*.py "$todir\dist\py-fosphor" 
    cp grc\*.xml "$todir\dist\blocks" 
    cd chocolatey
    choco pack
    cd ..
}else{
	mkdirp "build"
    cd build
    cmake .. `
	"-DBOOST_ROOT=$boost" `
	"-DBOOST_LIBRARYDIR=$boost\stage\lib" `
	"-DCMAKE_INCLUDE_PATH=$glew_include" `
	"-DCMAKE_FRAMEWORK_PATH=$glew_path" `
	"-DPKG_CONFIG_EXECUTABLE=$pkgconfig_exe" `
	"-DSWIG_DIR=$swig" `
	"-DSWIG_EXECUTABLE=$swig\swig.exe" `
	"-DFREETYPE2_DIR=$freetype" `
    "-DCPPUNIT_DIR=$cppunit" `
	"-DGLFW3_INCLUDE_DIR=$glfw\include" `
	"-DGLFW3_LIBRARY_DIR=$glfw\build\src\Release" `
	"-DGNURADIO_LIBRARY_DIR=$gnuradio/lib" `
	"-DGNURADIO_INCLUDE_DIR=$gnuradio/include" `

    cd ..
}
