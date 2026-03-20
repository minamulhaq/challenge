rm -rf build          
idf.py --preview set-target linux
idf.py build
./build/unit_tests.elf 