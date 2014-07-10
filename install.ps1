$gnuradio = "C:\Program Files (x86)\gnuradio"
$gnuradio_fosphor_python = "$gnuradio\lib\site-packages\gnuradio\fosphor"

if (-not (test-path $gnuradio_fosphor_python -pathtype container )) {
	mkdir $gnuradio_fosphor_python
}
cp build\lib\Release\gnuradio-fosphor.dll "$gnuradio\bin"
cp build\swig\Release\_fosphor_swig.pyd $gnuradio_fosphor_python
cp build\swig\fosphor_swig.py $gnuradio_fosphor_python
cp python\*.py $gnuradio_fosphor_python
cp grc\*.xml "$gnuradio\share\gnuradio\grc\blocks"
