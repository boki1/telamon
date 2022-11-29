#!/bin/sh

project_name="telamon"
project_root=${PWD}
src_dir=src
out_dir=build/out
docs_dir=build/docs
cc_compiler=g++-11
c_compiler=gcc-11

usage() {
	echo "Bad usage: ./build/abc.sh [build|test|clean|format|checkset|doc]"
}

make_check_settings() {
	helper_check_setting() {
		exe="$(which ${1})"
		[ -z "${exe}" ] && echo "${1}: Cannot find any executable file" || echo "${1}: Using ${exe}";	
	}

	helper_check_setting ${cc_compiler}
	helper_check_setting ${c_compiler}
	helper_check_setting cmake
	helper_check_setting ctest
	helper_check_setting doxygen
	[ -f build/conanfile.txt ] && helper_check_setting conan
}

# *Note* This is not a regular build, but a debug build instead.
# We are always building both the core library and the unit tests.
make_debug_build() {
	mkdir -p $out_dir
	[ -f build/conanfile.txt ] && conan install build/ -if $out_dir --build=missing
	cmake -S build/ -B $out_dir -G Ninja 			\
		-DCMAKE_C_COMPILER=${c_compiler} 		\
		-DCMAKE_CXX_COMPILER=${cc_compiler} 		\
		-DTELAMON_BUILD_TESTS=yes
	ninja -j $(nproc) -C $out_dir
}

make_run_tests() {
	ctest --test-dir $out_dir
}

make_clean() {
	rm -rf $out_dir
	rm -rf $docs_dir
}

make_format() {
	find ${project_root}/src \( -name "*.cc" -o -name "*.hh" \) -exec clang-format -i {} \;
}

make_documentation() {
	[ ! -f $docs_dir/Doxyfile ] && die "No Doxyfile present"
	doxygen $docs_dir/Doxyfile
}

main() {
	for arg in "$@"
	do
		case $arg in
		    "build") make_debug_build;;
		    "test") make_run_tests;;
		    "clean") make_clean;;
		    "format") make_format;;
		    "checkset") make_check_settings;;
		    "doc") make_documentation;;
		    *) usage && exit;;
		esac
	done
}

#
# Driver code
#

if [ $# -eq 0 ]; then
	usage
	exit 0
fi

# So far this is the only place in this script which I would like to have access to all those nice bashisms.
# However it seems a bit silly to me to require the whole bash-bloat for only pushd/popd, so here we are:
prev_cwd=${PWD}

project_root=$(git rev-parse --show-toplevel)
chdir ${project_root}
echo "Considering ${project_root} as the root of the project."
main $@
chdir ${prev_cwd}
