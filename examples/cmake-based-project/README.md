# Examples using CMake, CPM, and libvsync

Configure and build example

	cmake -S. -Bbuild
	make -C build

Run with locks:

	LD_PRELOAD=build/liblock.so build/example
	LD_PRELOAD=build/libmcslock.so build/example

